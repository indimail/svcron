<div align = center>

# Vixie Cron

**Cron** based on **Vixie Cron** that can load crontabs from any directory.

</div>

This version of **Cron** is functionally based on **Vixie Cron**'s implementation and thus allows every user to have their own **CronTab** file. **Cron** has been modifed to work with [supervise](https://github.com/indimail/indimail-mta/wiki/supervise.8). It goes without saying that you could use it without **supervise**. This version of cron, renamed as **svcron**, allows you to have crontabs in any directory. Similarly, the **crontab** command has been renamed as **svcrontab**, allowing you to edit crontabs in any directory. You are not restricted to hard coded, semi-configurable directories like <u>/var/spool/cron</u>. Both **svcron** and **svcrontab** have an additional <b>-d</b> argument to pass the crontabs directory as an argument. The functionality of **svcron** will be subsumed into supervise later.

Few of the standard I/O functions have been replaced by [substdio](https://github.com/indimail/indimail-mta/wiki/substdio.3) interface from [libqmail](https://github.com/indimail/libqmail), fixed size arrays replaced with [stralloc](https://github.com/indimail/indimail-mta/wiki/stralloc.3) variables, The plan is to replace everthing with functions from **libqmail**.

Standard Binary Packages will be made available for RPM/ArchLinux/Debian based systems using Open Build Service.

The changes to supervise are yet to be made and published. Watch out this space for updates. There are still few things to be ironed out (mostly security concerns) and testing.


## 📑  Crontabs

All crontab files, created without the <b>-d</b> option are stored in a read protected folders at <u>/var/spool/cron/crontabs</u>, You can have crontab files in any other directory under your control, using the <b>-d</b> option

<br>

## 📋  Logging

A message will be logged for every  command that is run by the **crontab** command.

<br>

## 🛡  Access

You can control access to the **crontab** command by utilizing the  **allow**  and  **deny**  files in <u>/var/spool/cron</u>.

<br>


<!----------------------------------------------------------------------------->

[#]: #

[Cron]: https://en.wikipedia.org/wiki/Cron
[Thanks]: Documentation/Thanks.md


<!-------------------------------{ Buttons }----------------------------------->
