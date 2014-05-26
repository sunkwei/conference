
rem stscht.bat <task id> <to run> <time>

schtasks /create /sc once /tn %1 /tr %2 /f /st %3
