## Introduction ##

This repository contains the source code of plugins that replace the default SQLite engine of Orthanc with a [PostgreSQL](http://www.postgresql.org/) back-end.

## Licensing ##

The code of the PostgreSQL plugins is licensed under the **Affero General Public License** ([AGPL](https://en.wikipedia.org/wiki/Affero_General_Public_License)). Pay attention to the fact that this license is more restrictive than the license of the [Orthanc core](http://orthanc.googlecode.com/).

## Compilation ##

The procedure to compile these plugins is similar of that for the [core of Orthanc](https://code.google.com/p/orthanc/source/browse/INSTALL?name=Orthanc-0.8.6). The following commands should work for every UNIX-like distribution (including Linux):

```
# mkdir Build
# cd Build
# cmake .. -DSTATIC_BUILD=ON
# make
```

The compilation will produce 2 shared libraries, each containing one plugin for Orthanc:
  * `OrthancPostgreSQLIndex` replaces the default SQLite index of Orthanc by PostgreSQL.
  * `OrthancPostgreSQLStorage` makes Orthanc store the DICOM files it receives into PostgreSQL.

## Usage ##

You of course first have to install Orthanc, with a version above 0.8.6.

You then have to create a database dedicated to Orthanc on some PostgreSQL server. Please refer to the [PostgreSQL documentation](http://www.postgresql.org/docs/9.1/static/tutorial-createdb.html) for this task.

Once Orthanc is installed and the database is created, you have to add a section in the [configuration file of Orthanc](https://code.google.com/p/orthanc/wiki/OrthancConfiguration) that specifies the address of the PostgreSQL server together with your credentials. You also have to tell Orthanc in which path it can find the plugins: This is done by properly modifying the "`Plugins`" option. You could for instance use the following configuration file:

```json

{
"Name" : "MyOrthanc",
[...]
"PostgreSQL" : {
"EnableIndex" : true,
"EnableStorage" : true,
"Host" : "localhost",
"Port" : 5432,
"Database" : "orthanc",
"Username" : "orthanc",
"Password" : "orthanc"
},
"Plugins" : [
"/home/user/OrthancPostgreSQL/Build/libOrthancPostgreSQLIndex.so",
"/home/user/OrthancPostgreSQL/Build/libOrthancPostgreSQLStorage.so"
]
}
```

Note that `EnableIndex` and `EnableStorage` must be explicitly set to `true`, otherwise Orthanc will continue to use its default SQLite back-end.

Orthanc must of course be restarted after the modification of its configuration file. The log will contain an output similar to:

<pre>
# ./Orthanc Configuration.json<br>
W0212 16:30:34.576972 11285 main.cpp:632] Orthanc version: 0.8.6<br>
W0212 16:30:34.577386 11285 OrthancInitialization.cpp:80] Using the configuration from: Configuration.json<br>
[...]<br>
W0212 16:30:34.598053 11285 main.cpp:379] Registering a plugin from: /home/jodogne/Subversion/OrthancPostgreSQL/Build/libOrthancPostgreSQLIndex.so<br>
W0212 16:30:34.598470 11285 PluginsManager.cpp:258] Registering plugin 'postgresql-index' (version 1.0)<br>
W0212 16:30:34.598491 11285 PluginsManager.cpp:148] Using PostgreSQL index<br>
W0212 16:30:34.608289 11285 main.cpp:379] Registering a plugin from: /home/jodogne/Subversion/OrthancPostgreSQL/Build/libOrthancPostgreSQLStorage.so<br>
W0212 16:30:34.608916 11285 PluginsManager.cpp:258] Registering plugin 'postgresql-storage' (version 1.0)<br>
W0212 16:30:34.608947 11285 PluginsManager.cpp:148] Using PostgreSQL storage area<br>
[...]<br>
W0212 16:30:34.674648 11285 main.cpp:530] Orthanc has started<br>
</pre>

Instead of specifying explicit authentication parameters, you can also use [the PostgreSQL connection URIs](http://www.postgresql.org/docs/9.4/static/libpq-connect.html#LIBPQ-CONNSTRING). For instance:

```json

{
"Name" : "MyOrthanc",
[...]
"PostgreSQL" : {
"EnableIndex" : true,
"EnableStorage" : true,
"ConnectionUri" : "postgresql://username:password@localhost:5432/database"
},
"Plugins" : [
"/home/user/OrthancPostgreSQL/Build/libOrthancPostgreSQLIndex.so",
"/home/user/OrthancPostgreSQL/Build/libOrthancPostgreSQLStorage.so"
]
}
```

## Locking ##

By default, the plugins lock the database (using [PostgreSQL advisory locks](http://www.postgresql.org/docs/9.4/static/functions-admin.html#FUNCTIONS-ADVISORY-LOCKS)) to prevent other instances of Orthanc from using the same PostgreSQL database. If you want several instances of Orthanc to use the same database, set the "`Lock`" option to `false` in the configuration file:

```json

{
"Name" : "MyOrthanc",
[...]
"PostgreSQL" : {
"EnableIndex" : true,
"EnableStorage" : true,
"Lock" : false,
"ConnectionUri" : "postgresql://username:password@localhost:5432/database"
},
"Plugins" : [
"/home/user/OrthancPostgreSQL/Build/libOrthancPostgreSQLIndex.so",
"/home/user/OrthancPostgreSQL/Build/libOrthancPostgreSQLStorage.so"
]
}
```