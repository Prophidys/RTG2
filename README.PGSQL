From Chris Caputo <ccaputo@alt.net>:

Before running targetmaker, create your rtg database and add the following
to it:

--
  CREATE TABLE router (
    rid SERIAL NOT NULL,
    name char(120) NOT NULL default '',
    pop char(10) NOT NULL default '',
    popid smallint NOT NULL default '0'
  );
  CREATE INDEX router_idx ON router (rid);

  CREATE TABLE interface (
    id SERIAL NOT NULL,
    name char(255) NOT NULL default '',
    rid int NOT NULL default '0',
    speed bigint default NULL,
    description char(255) default NULL,
    status boolean default true
  );
  CREATE INDEX interface_idx ON interface (id);
--

Be sure to change DB_TYPE in targetmaker.cfg as follows:

  $DB_TYPE = "Postgres";

And adjust rtg.conf as follows:

  DB_Driver   librtgpgsql.so
  Threads 1

(I only tested single-threaded mode.)


