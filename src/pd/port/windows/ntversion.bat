@echo off
rem ***********************************************************************
rem ** bug-25127 windows server 2008 porting
rem ** NT majoer ���� ��ȣ�� �������� 5�� �����Ǿ� �־��µ� 
rem ** 2008���� NT 6�̹Ƿ� ver.exe ���α׷����� NT ������ ���Ͽ� 
rem ** ��Ű�������� ����ϵ��� �Ѵ�.
rem ** ver.exe ����� ���: Microsoft Windows [Version 6.0.6002]
rem ** Version 6�� ã���� idConfig.h ���ϵڿ� OS_MAJORVER�� 6���� �߰�
rem ** ��ã���� ������� 5�� �߰��Ѵ�.
rem ** �� bat ������ configure64.bat (or configure.bat)���� ȣ���Ѵ�.
rem ***********************************************************************

ver | findstr /c:"Version 6" > NUL
if %errorlevel% equ 0 goto WINNT6

echo #define OS_MAJORVER 5 >> %ALTIBASE_DEV%\src\id\idConfig.h
goto EOF

:WINNT6
echo #define OS_MAJORVER 6 >> %ALTIBASE_DEV%\src\id\idConfig.h

:EOF
@echo on
