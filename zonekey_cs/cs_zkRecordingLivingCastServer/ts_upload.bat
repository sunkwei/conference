@echo off

rem 使用 lftp 上传，注意 lftp.exe 必须在 PATH 环境变量中
lftp -f %2
IF %errorlevel% NEQ 0 GOTO err

rem 成功，删除本地录像目录，删除 lftp 脚本，删除计划任务
rd /s/q %3
del %3.lftp
schtasks /delete /tn %1 /f
goto end

:err
rem 失败，滞后5分钟，重试
set t=%time%
set /a hour=%t:~0,2%
set /a min=%t:~3,2%
set /a min=%min%+5

if %min% gtr 59 set /a hour=(%hour%+1)%%24&set /a min=%min%%%60
if %hour% lss 10 set hour=0%hour%
if %min% lss 10 set min=0%min%
set t=%hour%:%min%

schtasks /delete /tn %1 /f
schtasks /create /sc once /tn %1 /tr "%0 %1 %2 %3" /f /st %t% 

:end
echo "End"
"End"
