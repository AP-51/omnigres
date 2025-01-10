create function instantiate(schema regnamespace default 'omni_test') returns void
    language plpgsql
as
$$
begin
    -- Set the search path to target schema and public
    perform
        set_config('search_path', schema::text || ',public', true);


    -- Ensure we have omni_test role (these are cluster-wide)
    perform from pg_roles where rolname = 'omni_test';
    if not found then
        -- We set password to omni_test; this is not expected to be used in production
        -- but used to ensure we have stable connection parameters.
        -- We also grant it superuser privileges as tests might require that.
        create role omni_test superuser password 'omni_test';
    end if;

    create type test as
    (

    );
    execute format('alter type test set schema %s', schema);

    -- Test report
    create type test_report as
    (
        name          text,
        description   text,
        start_time    timestamp,
        end_time      timestamp,
        error_message text
    );

    -- Utility functions
    create function _omni_test_dropdb_helper(self_conn text, db name) returns void
        language plpgsql
    as
    $_omni_test_dropdb_helper$
    declare
        conn text := 'omni_test_drop_' || db;
    begin
        if current_setting('server_version') ~ '^(15|16).*' then
            perform dblink_connect(conn, self_conn);
            perform dblink_send_query(conn, format('drop database if exists %I', db));
            while dblink_is_busy(conn)
                loop
                    null;
                end loop;
            perform dblink_disconnect(conn);
        else
            perform dblink(self_conn, format('drop database if exists %I', db));
        end if;
    end;
    $_omni_test_dropdb_helper$;

    -- Create test runner
    create function run_tests(db name, out test_report test_report) returns setof test_report
        language plpgsql as
    $run_tests$
    declare
        run            int  := 1;
        test_run       text := 'omni_test_' || run;
        conn           text := 'omni_test_' || run;
        host           text := ' host=' ||
                               current_setting('unix_socket_directories') || ' port=' || current_setting('port') ||
                               ' user=' || current_user;
        self_conn      text := 'dbname=' || current_database() || host;
        template_conn  text := 'dbname=' || db || host;
        rec            record;
        -- TODO: temporary workaround until we have omni_service fully integrated
        has_omni_httpd bool;
    begin
        -- We must only run this off a template database so we can easily clone it.
        -- Technically, it doesn't have to be a template one – but this further enforces
        -- the notion that this is not a working database (for example, `omni_httpd`
        -- does not start the server in a template database)
        perform from pg_database where datname = db and datistemplate;
        if not found then
            raise exception 'Selected database (%) must be a template database', db;
        end if;


        select
            present
        into has_omni_httpd
        from
            dblink(template_conn, $sql$
            select true from pg_extension where extname = 'omni_httpd'
            $sql$) as t(present bool);

        -- Clean up the test run database if one is left over
        -- TODO: stop services uniformly when this will become possible
        perform from pg_database where datname = test_run;
        if found and has_omni_httpd then
            perform dblink_exec('dbname=' || test_run || host, 'call omni_httpd.stop()');
            perform _omni_test_dropdb_helper(self_conn, test_run);
        end if;

        begin

            -- Find qualifying tests
            for rec in
                select *
                from
                    dblink(template_conn,
                           $sql$
                           select
                               pg_proc.prokind,
                               pg_proc.proname,
                               ns.nspname,
                               d.description
                           from
                               pg_proc
                                   inner join pg_namespace ns on ns.oid = pg_proc.pronamespace
                                   left join pg_description d on d.objoid = pg_proc.oid
                           where
                               (cardinality(proargtypes) = 1 and proargtypes[0] = 'omni_test.test'::regtype
                                   and prokind = 'p') or
                               (cardinality(proargtypes) = 0 and
                                   prorettype = 'omni_test.test'::regtype)
                           $sql$) t(prokind "char", proname name, nspname name, description text)
                loop
                    -- Clone the database
                    perform dblink(self_conn, format('create database %I template %I', test_run, db));
                    -- Connect to it
                    perform dblink_connect(conn, 'dbname=' || test_run || host);

                    test_report.name = rec.nspname || '.' || rec.proname;
                    test_report.description = rec.description;

                    -- Run the test
                    test_report.start_time := clock_timestamp();
                    begin
                        if rec.prokind = 'p' then
                            perform *
                            from
                                dblink(conn, format('call %I.%I(null)', rec.nspname, rec.proname)) r(result test);
                        else
                            perform *
                            from
                                dblink(conn, format('select %I.%I()', rec.nspname, rec.proname)) r(result test);
                        end if;
                        raise notice 'ok – %', test_report.name;
                    exception
                        when others then
                            raise notice 'failure – % – %', test_report.name, sqlerrm;
                            test_report.error_message = sqlerrm;
                            null;
                    end;
                    test_report.end_time := clock_timestamp();
                    return next;
                    test_report.error_message = null;

                    -- TODO: stop services uniformly when this will become possible
                    if has_omni_httpd then
                        perform dblink_exec(conn, 'call omni_httpd.stop()');
                    end if;
                    perform dblink_disconnect(conn);
                    perform _omni_test_dropdb_helper(self_conn, test_run);
                end loop;
        exception
            when others then
                perform from unnest(dblink_get_connections()) t(name) where name = conn;
                if found then
                    perform dblink_disconnect(conn);
                end if;
                -- TODO: stop services uniformly when this will become possible
                if has_omni_httpd then
                    perform dblink_exec('dbname=' || test_run || host, 'call omni_httpd.stop()');
                end if;
                perform _omni_test_dropdb_helper(self_conn, test_run);
                raise;
        end;
        return;

    end;
    $run_tests$;

    execute format('alter function run_tests set search_path to %I,public', schema);


end;
$$;