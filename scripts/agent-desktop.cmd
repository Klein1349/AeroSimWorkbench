@echo off
setlocal
set "AGENT_DESKTOP_CLI=%~dp0..\tools\agent-desktop\node_modules\.bin\agent-desktop.cmd"
if not exist "%AGENT_DESKTOP_CLI%" (
    echo Run scripts\setup-agent-desktop.ps1 first. 1>&2
    exit /b 1
)
call "%AGENT_DESKTOP_CLI%" %*
exit /b %ERRORLEVEL%
