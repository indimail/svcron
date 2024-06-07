
<br>

<div align = center>

# Vixie Cron

*The **[Cron]** flavor that runs on most systems.*

<br>
<br>

This version of **Cron** is functionally based on <br>
**System V**'s implementation and thus allows <br>
every user to have their own **CronTab** file.

</div>

## 📑  Crontabs

All crontab files are stored in a read  
protected folders at  `/var/spool/cron/crontabs` 

<br>

## 📜  At

**There is no direct support for  `at`**

However as long as your system  
supports it, you can still use  `atrun`

<br>

## 📋  Logging

A message will be logged for every  
command that is run by a CronTab.

<br>

## 🛡  Access

You can control access to the  `crontab`  
command by utilizing the  `allow`  and  
`deny`  files in  `/var/spool/cron`

*The command is used to install crontabs.*

<br>

## 📺  System V

While it hasn't been tested yet, some effort <br>
has gone into making porting to it easier.

<br>


<!----------------------------------------------------------------------------->

[#]: #

[Cron]: https://en.wikipedia.org/wiki/Cron

[Conversion]: Documentation/Conversion.md
[Configure]: Documentation/Configure.md
[Features]: Documentation/Features.md
[Changes]: Documentation/Changelog.md
[Thanks]: Documentation/Thanks.md
[Mails]: Documentation/Mail.md


<!-------------------------------{ Buttons }----------------------------------->
