-- Upsert functions ("insert or replace")
-- http://www.postgresql.org/docs/current/static/plpgsql-control-structures.html#PLPGSQL-UPSERT-EXAMPLE
CREATE FUNCTION ChangeMetadata(id_ BIGINT, type_ INT, value_ TEXT) RETURNS VOID AS
$$
BEGIN
    LOOP
        -- first try to update the property
        UPDATE Metadata SET value = value_ WHERE id = id_ AND type = type_;
        IF found THEN
            RETURN;
        END IF;
        -- not there, so try to insert the property
        -- if someone else inserts the same property concurrently,
        -- we could get a unique-key failure
        BEGIN
            INSERT INTO Metadata VALUES (id_, type_, value_);
            RETURN;
        EXCEPTION WHEN unique_violation THEN
            -- Do nothing, and loop to try the UPDATE again.
        END;
    END LOOP;
END;
$$
LANGUAGE plpgsql;
