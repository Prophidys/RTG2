[![Build Status](https://travis-ci.org/Prophidys/RTG2.svg?branch=master)](https://travis-ci.org/Prophidys/RTG2)


RTG README
==========

Contents:
 1. Introduction
 2. Prerequisites
 3. Quick start
 4. Compiling 
 5. Configuration
 6. Monitoring Gauges
 7. Graphs and HTML Generation
 8. Generating Reports
 9. Maintenance
 10. Troubleshooting
 11. OS-Specific Configuration
 12. Security
 13. Installing the MySQL Perl DBI
 14. Future
 15. Acknowledgments



1. Introduction
---------------

RTG is a flexible, scalable, high-performance SNMP statistics monitoring
system.  It is designed for enterprises and service providers that need to
collect time-series SNMP data for a large number of objects from a large
number of network elements quickly.  All collected data is inserted into a
MySQL database allowing sophisticated analysis and reporting.  The package
includes utilities that generate RTG configuration files, HTML, sample
reports and graphical data plots. RTG is freely available under the terms
of the GNU General Public License.

The primary advantages of RTG are:
  - Runs as a daemon, incurring no cron or kernel startup overhead
  - Written entirely in C for speed
  - Can poll at sub one-minute intervals
  - No averaging
  - Inserts data into a relational database where complex queries and 
    reports may be generated

If these features are not important to you, RTG is likely the wrong tool
for the job.  Alternatives we highly recommend are MRTG, RRDtool and
Cricket.  Please see the FAQ.


2. Prerequisites
----------------

RTG comes as C source code and is intended to run on UNIX systems.  RTG
requires a UNIX system with POSIX thread support, 64-bit long integers and
a sane compiler (gcc works great).  RTG requires two external packages:
  - MySQL      (http://www.mysql.com/) 
  - Net SNMP   (http://net-snmp.sourceforge.net/)

RTG can also use the older UCD SNMP, but we recommend sticking with 
Net SNMP.

To use the built in reports and configuration generators and to generally
get the most out of RTG, it is strongly recommended that you also install:
  - Apache     (http://www.apache.org)
  - PHP        (http://www.php.net)
  - Perl DBI   (http://www.mysql.com/downloads/api-dbi.html)


3. Quick start
--------------

RTG is complex to setup, so please read all of the documentation.  

### Build RTG and install into default location /usr/local/rtg2:

    $ git clone git@github.com:Prophidys/RTG2.git
    $ cd RTG2/
    $ ./boostrap.sh
    $ ./configure
    $ make
    # make install
    
### Make certain the MySQL database is installed and running.

### Configure the database.  You must know the MySQL root password.
     The createdb script will setup the database for RTG:

    $ /usr/local/rtg2/etc/createdb mysqlroot

### Edit the /usr/local/rtg2/etc/routers file, adding each router you wish
     to SNMP poll, one per line.  To specify a per-router non-default 
     community string, add "router:community" in the routers file.
     To specify a per-router non-default response bit width, add
     "router:community:bits" in the routers file.
     
###  Run the /usr/local/rtg2/etc/rtgtargmkr.pl script to manage the RTG
target file.  The first run will create a targets.cfg file. 

### Start up the poller to use the target list:

    $ /usr/local/rtg2/bin/rtgpoll -v -t targets.cfg

If the poller does not find a configuration file, it will create
one in the current directory called "rtg.conf".  You want to
modify this file to suit your installation.  If the poller is
successful, the "Polls" counter in the statistics banner will
increase and then a countdown to the next poll is displayed.  
The DBInserts should increment after the second polling round.  

### RTG is running.  Manipulate or mine the data in the database as 
     needed, or read the remaining sections for graphing and reporting 
     instructions.


4. Compiling
------------

Please also see the OS-Specific configuration section of this document
for details on building RTG2 on specific platforms.

RTG uses GNU autoconf to determine your build type.  Compiling should be a
simple matter of:

    ./configure
    make 
    make install

See "./configure --help" for a complete list of build options.

RTG requires the Net SNMP and thread-safe MySQL libraries; compilation
will fail if configure cannot find these libraries.

Newer MySQL binary distributions include the thread-safe libraries by 
default.  If you compile your own MySQL, be certain to include the 
"--enable-thread-safe-client" option when configuring.  If you are uncertain, 
check to see if you have a "libmysqlclient_r*" library installed on your 
system; this is the reentrant, thread-safe library.

The MySQL database may reside on a different machine than RTG, however RTG
still needs the MySQL libraries.  If your MySQL libraries are not in a
standard location, use the --with-mysql=/mysqlpath argument with
configure.  If your SNMP libraries are not in a standard location, use the
--with-snmp=/snmppath argument with configure.

RTG has successfully built on: Solaris 2.x, Linux 2.x, and FreeBSD 4.x
Please let us know if you build RTG on other platforms and what
modifications were required.  If you experience trouble compiling RTG, see
the Troubleshooting section of this document.


5. Configuration
----------------

rtgpoll takes a number of options, rtgpoll -h displays the options.
rtgpoll uses two ASCII files to direct its actions: a configuration file
and a target file.  The configuration file specifies general RTG
configuration parameters while the target file lists the devices that RTG
will SNMP poll.  The default name of the configuration file is rtg.conf.

A single rtg.conf file can control all programs (including rtgpoll, 
rtgplot, *.php and *.pl).  Each program attempts to find an rtg.conf
in the following path order:
 1. ./rtg.conf
 2. /usr/local/rtg2/etc/rtg.conf
 3. /etc/rtg.conf

If rtgpoll or rtgplot cannot find an rtg.conf file in any of these 
paths, it will attempt to create one in the current working directory.
Most users maintain a master rtg.conf file customized for their 
environment.  rtg.conf contains the following configurable fields:

    Interval         300
    HighSkewSlop     3
    LowSkewSlop      .5
    OutOfRange       93750000000
    SNMP_Ver         1
    SNMP_Port       161
    DB_Host          localhost
    DB_Database      rtg
    DB_User          snmp
    DB_Pass          rtgdefault
    Threads          5

Interval is the time between successive polls of the target list,
default is 300 seconds (5 minutes).

HighSkewSlop defines the maximum number of Intervals allowed between two
consecutive poll values before the time in seconds between said points is
deemed too large to calculate a valid rate. With the default Interval and
HighSkewSlop values, that time would be 300 * 3 (15 minutes).

LowSkewSlop defines the minimum number of Intervals alloweed between two
consecutive poll values before the time in seconds between said points is
deemed two low to calculate a valid rate. With the default Interval and
LowSkewSlop values, that time would be 300 * 0.5 (2.5 minutes).

OutOfRange defines an upper bound above which rtgpoll will never attempt
an insert into the database. OutOfRange should be a multiple of the
maximum number of bytes possible in the defined Interval for your
highest speed link.  The default OutOfRange value will suffice in most
installations. OutOfRange may be specified on a per OID basis in the
'targets.cfg' file. OOR values are inserted after the interface ID and
before the description. The default value entered by rtgtargmkr.pl is basically the interface's ifSpeed value multiplied by the Interval specified in rtg.conf. Legal values for OOR are 30 or less numeric characters. As of RTG-0.8, the
OutOfRange value should be defined on a per target basis.

SNMP_Ver specifies the SNMP version the poller will use. As of RTG-0.8, the
SNMP_Ver should be specified on a per host basis.

The number of threads rtgpoll will use is defined in the variable
Threads.

Variables in rtg.conf must match the names above exactly.  Comments
and blank lines are allowed and the ordering of variables in rtg.conf
does not matter.

The target file specifies the objects to be SNMP polled.  Comments must be
preceded with a '#' sign.  Elements in the target file are tab delimited.  
The format of the target file is fixed for two possible configurations:

PRE RTG-0.8 Format:
  # Host  OID  64/32  Community  Table   ID   Description

where 

    Host        = IP or hostname of target
    OID         = Full SNMP OID, e.g. .1.3.6.1.2.1.31.1.1.1.10.19
    64/32/0     = Specify 64/32 bit objects or 0 for gauge objects
    Community   = SNMP Community
    Table       = MySQL table in the database to use
    ID          = A unique ID that is used with each insert
    OOR         = The maximum delta of an object's counter within one interval.
    Description = Free text

RTG-0.8 and beyond (PRE RTG-0.8 Format Is Still Supported):
host [hostname] {
	community [communityString];
	snmpver [1|2];
	target [OID] {
		bits [64|32|0];
		table [tableName];
		id [idnum];
		speed [OORValue];
		descr "[description]";.
	};
	target [OID] {
		...
		...
	};
	...
	...
};

rtgpoll first reads the configuration file, then the target file.  For
each SNMP poll, rtgpoll will attempt an SQL INSERT of the form:

  INSERT INTO Table VALUES (ID, NOW(), bigint)

Where Table is the name of the database table and ID is an integer.  Both
Table and ID come from the target list, NOW() is the current timestamp and
bigint is the delta value between successive SNMP polls.

RTG makes no attempt at determining rate; one must look at the time
difference successive entries in the database.  The RTG graphing and
reporting tools automatically calculate rate.


6. Monitoring Gauges
--------------------

RTG can monitor OIDs that return a gauge value allowing one to monitor
items such as temperature, CPU, users, etc.  If an entry in the target
file has 0 (zero) specified as the OID bit width instead of 64 or 32,
the poller assumes that it is monitoring a gauge and does not attempt
to calculate an interval delta value.  Currently there are no
automated scripts to generate a gauge target file, but should be
trival for users to implement for their environment. Consequently, routers
in the 'routers' file should not have '0' bit width specified unless
said trivial changes are implemented.


7. Graphs and HTML Generation
-----------------------------

RTG includes PHP pages to serve interactive graphical traffic plots via
the web.  Using PHP demonstrates RTG best; we strongly recommend using
this method.  Note that we assume an Apache (http:/www.apache.org) web
server with PHP and MySQL support running on a UNIX system.

To configure an apache web server to serve graphical RTG plots via PHP:

  a. Build a PHP-capable Apache server (see http://www.php.net)
  b. Add "SetEnv LD_LIBRARY_PATH /usr/local/lib:/usr/local/lib/mysql" 
     to httpd.conf (Or wherever your MySQL libraries are found).
  c. Uncomment (or possibly add) "AddHandler cgi-script .cgi" in httpd.conf
  d. Add "ExecCGI" to the htdocs Options statement in httpd.conf 
  e. Uncomment (or possibly add) "AddType application/x-httpd-php .php"
  f. Restart apache
  g. Copy the PHP files and images to the htdocs directory:
        cp /usr/local/rtg/web/* /usr/local/apache/htdocs
  h. Copy the rtgplot binary to the htdocs directory and *add* .cgi
     extension:
        cp /usr/local/rtg/bin/rtgplot /usr/local/apache/htdocs/rtgplot.cgi
  i. Point your web browser at the URL (substitute your site in):
        http://mysite.com/rtg.php  (Interactive Reports)
        http://mysite.com/95.php   (95th Percentile Queries)
        http://mysite.com/view.php (MRTG-style overview + day/wk/mo plots)

The rtgplot executable generates traffic graphs in PNG format.  You do not
have to use PHP or our included PHP web pages. One can easily construct
custom web pages by using the proper HTML tags to embed an RTG plot
anywhere.  An RTG image can be placed in any web page by using an < IMG >
tag with the appropriate values.  For example:

<IMG SRC="rtgplot.cgi?t1=ifInOctets_2&t2=ifOutOctets_2&iid=49&begin=1028606400&end=1028692799&units=bits/s&factor=8&scalex=yes">

will plot two lines from the MySQL tables ifInOctets_2 and ifOutOctets_2
corresponding to interface 49 on router 2 for the time span 1028606400 to
1028692799 (UNIX epoch seconds).  The factor argument allows for bits per
second; the units argument is displayed as the Y-axis label on the plot.
The scalex argument auto-adjusts the X time axis according to the available
data samples rather than according to the actual time span given.  If you
are plotting non-continuous data, such as error plots, use the impulses=yes
argument.


8. Generating Reports
---------------------

RTG includes two Perl scripts in $prefix/bin that generate per-customer
traffic reports.  These Perl scripts use the Perl-MySQL DBI (see the
prerequisites section for details).  The first script is report.pl:

USAGE: ./report.pl <customer> -[##d][##h][##m][##s]
   OR: ./report.pl <customer> <mm/dd/yyyy[+hh:mm[:ss]]> <mm/dd/yyyy[+hh:mm[:ss]]>

Give report a customer name or partial customer name as it appears on the
interface description of the network device and report will generate a
traffic report for all matching customers in the network. For example:

Blah Industries Traffic
Period: [08/05/2002 00:00 to 08/05/2002 23:59]

                             In      Out  Avg In Avg Out   Util   Util  Max In Max Out Max Ut Max Ut
Connection               MBytes   MBytes    Mbps    Mbps   In %   Out%    Mbps    Mbps    In%   Out%
-------------------------------------------------------------------------------------------------------
at-1/2/0.104 core1.abc       18       20    0.01    0.01   0.01   0.01    0.01    0.01   0.01   0.01
at-1/2/0.109 core1.xyz      411      586    0.11    0.16   0.07   0.10    0.43    0.50   0.28   0.32

The second script is 95.pl:

USAGE: ./95.pl <customer> -[##d][##h][##m][##s]
   OR: ./95.pl <customer> <mm/dd/yyyy[+hh:mm[:ss]]> <mm/dd/yyyy[+hh:mm[:ss]]>

The 95.pl script generates 95th percentile traffic rates for a particular
customer for the previous day.  For example:

Blah Industries Traffic
Period: [08/05/2002 00:00 to 08/05/2002 23:59]

                         RateIn   RateOut    MaxIn   MaxOut   95% In  95% Out
Connection                 Mbps      Mbps     Mbps     Mbps     Mbps     Mbps
-------------------------------------------------------------------------------
at-1/2/0.104 core1.abc     0.01      0.01     0.01     0.01     0.01     0.01
at-1/2/0.109 core1.xyz     0.11      0.16     0.43     0.50     0.33     0.45

It is convenient to supply these Perl scripts the proper arguments in a
cron job in order to take automated actions (email, bill, report, etc) 
each night or week.

A sufficiently capable Perl hacker should be able to modify these example
scripts to suit the particular needs of any organization.  We appreciate
any contributed Perl reporting scripts (or clean up our messy Perl).


9. Maintenance
--------------

Most installations run the rtgtargmkr.pl script regularly via a cron job.  
rtgtargmkr will add new routers and interfaces as they appear in the
network (defined by the routers file).  The script will also detect
interface name changes and suggest the proper SQL command to update the
interface description in the database (this is done as a safety mechanism
to have a human involved in the updating process).

rtgpoll accepts a number of signals.  SIGHUP forces a reload of the target
file.  If rtgpoll is actively polling, the target file is reloaded when
rtgpoll becomes idle.  This is useful when automating the target list
creation based on your active network.  SIGUSR1 increases the verbosity of
a running rtgpoll; SIGUSR2 decreases the verbosity.

A typical installation will run the rtgtargmkr script nightly and then
send rtgpoll a -HUP signal to force the poller to re-read the target list.


10. Troubleshooting
-------------------

If you are experiencing problems with RTG, please first consult the FAQ.  
You may find the RTG mailing list archives helpful.  Questions not
answered by these two means should be sent to the mailing list
(http://fireflynetworks.net/mailman/listinfo/rtg).  When submitting
questions to the mailing list, please be sure to include the RTG version,
your machine architecture, and target file size.

RTG is compiled with lots of debug information.  Pass multiple '-v' flags
on the rtgpoll argument line to increase the verbosity.  Often this will
help you (or the developers) solve the problem.


11. OS-Specific Configuration
-----------------------------

FreeBSD: 
 a. The FreeBSD ports are the easiest way to install the required MySQL and
 SNMP libraries.  Simply enter the MySQL 4.1 directory in the port tree 
 "/usr/ports/databases/mysql41-client" and issue "make install".  To use
 MySQL 3.x the build requires additional options.  To install the MySQL
 thread-safe libraries in 3.x, issue "make THREAD_SAFE_CLIENT=yes all install" 
 from within the "/usr/ports/databases/mysql323-server".  Alternatively,
 if you already have MySQL installed but not the thread safe libraries, 
 run "make install" from 
 "/usr/ports/databases/mysql323-server/work/mysql-3.23.58/libmysql_r".

 b. Install Net-SNMP with "make all install" from the "/usr/ports/net/net-snmp"
 directory.  A FreeBSD port should be along soon.  FreeBSD rocks.
 
 c. To use the included Perl scripts, install the DBI and DBD ports:
 "/usr/ports/databases/p5-DBI" and "/usr/ports/databases/p5-DBD-mysql"


Linux:
 Linux users using RPMs of SNMP and MySQL need to install both the
 util and devel RPMs of each package.  From example:
   Net-SNMP: net-snmp-*, net-snmp-devel-*, net-snmp-utils-* 
   MySQL:    MySQL-client-*, MySQL-shared-*, MySQL-devel-*, MySQL-*


12. Security
------------

RTG uses MySQL with a default password of "rtgdefault" solely to 
ease installation.  The password should be changed as soon as RTG is 
installed and operational.  In addition, the MySQL server port
should be open only to trusted hosts and networks.  *The default
RTG installation can leave your statistics data vulnerable!*  Please 
refer to the MySQL website for details on securing your database.


13. Installing the MySQL Perl DBI
---------------------------------

The Perl reports and Perl configuration generators use the Perl DBI to
interface with the MySQL database.

 a. From http://www.mysql.com/downloads/api-dbi.html download:
      - DBI-x.xx.tar.gz
      - DBD-mysql-x.xxxx.tar.gz

 b. Uncompress and untar both packages:
      $ gzip -d DBI-x.xx.tar.gz; gzip -d DBD-mysql-x.xxxx.tar.gz
      $ tar -xvf DBI-x.xx.tar; tar -xvf DBD-mysql-x.xxxx.tar
 c. Build DBI package:
      $ cd DBI-x.xx; perl Makefile.PL; make; make install
 d. Build the MySQL DBD package:
      $ cd DBD-mysql-x.xxxx; perl Makefile.PL; make; make install


14. Future
----------

RTG is intended to be a foundation upon which to build.  Modules, features 
and patches are always welcome and appreciated.  To contribute, see 
the "TODO" file.


15. Acknowledgments
-------------------

RTG was inspired by the likes of MRTG, RRDtool (Tobias Oetiker, Dave Rand)
and Cricket (Jeff Allen).  RTG uses (and useless without) the excellent 
work of many others, including: MySQL, UCD/Net-SNMP, gd, png, cgilib
and SNMP_Session.  Thanks to SourceForge (http://www.sf.net) for providing
an excellent service to the developer community.  Finally, thanks to all
those who have tested and provided patches or suggestions to RTG!
