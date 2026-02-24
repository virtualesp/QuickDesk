@echo off

echo=
echo=
echo ---------------------------------------------------------------
echo check ENV
echo ---------------------------------------------------------------

:: example: C:\QtPro\6.8.4
set ENV_QT_PATH=C:\QtPro\6.8.4
echo ENV_QT_PATH %ENV_QT_PATH%

:: 获取脚本绝对路径
set script_path=%~dp0
:: 进入脚本所在目录,因为这会影响脚本中执行的程序的工作目录
set old_cd=%cd%
cd /d %~dp0

:: 启动参数声明和默认值
SETLOCAL EnableDelayedExpansion
set cpu_mode=x64
set build_mode=Release
set clean_output=false
set errno=1

echo=
echo=
echo ---------------------------------------------------------------
echo 解析命令行参数
echo ---------------------------------------------------------------

:: 遍历所有参数
:: 说明：%1 始终代表当前第一个参数，shift 命令会将所有参数向左移动一位
:: 例如：build_qd_win.bat release clean -> %1=release, shift后 -> %1=clean
:parse_args
if "%1"=="" goto args_done

REM 检查编译类型（不区分大小写）
if /i "%1"=="debug" set build_mode=Debug
if /i "%1"=="release" set build_mode=Release
if /i "%1"=="minsizerel" set build_mode=MinSizeRel
if /i "%1"=="relwithdebinfo" set build_mode=RelWithDebInfo

REM 检查是否需要清理
if /i "%1"=="clean" set clean_output=true

shift
goto parse_args
:args_done

echo [*] 编译类型: %build_mode%
echo [*] 清理输出目录: %clean_output%
echo=

set cpu_mode=x64
set cmake_vs_build_mode=x64
set qt_cmake_path=%ENV_QT_PATH%\msvc2022_64

echo=
echo Qt cmake 路径: %qt_cmake_path%

echo=
echo=
echo ---------------------------------------------------------------
echo 开始 CMake 构建
echo ---------------------------------------------------------------

:: 处理输出目录
set output_path=%script_path%..\output
if "!clean_output!"=="true" (
    if exist %output_path% (
        echo [*] 清理输出目录: %output_path%
        rmdir /q /s %output_path%
    )
) else (
    echo [*] 保留输出目录: %output_path%
)

:: 处理临时目录
set temp_path=%script_path%..\build-temp
if "!clean_output!"=="true" (
    if exist %temp_path% (
        echo [*] 清理临时目录: %temp_path%
        rmdir /q /s %temp_path%
    )
) else (
    echo [*] 保留临时目录（增量编译）: %temp_path%
)

:: 确保临时目录存在
if not exist %temp_path% (
    md %temp_path%
)
cd %temp_path%

set cmake_params=-DCMAKE_PREFIX_PATH=%qt_cmake_path% -DCMAKE_BUILD_TYPE=%build_mode% -G "Visual Studio 17 2022" -A %cmake_vs_build_mode%
echo [*] CMake 参数: %cmake_params%
echo=

cmake %cmake_params% ../
if not %errorlevel%==0 (
    echo [?] CMake 配置失败
    goto return
)

echo=
echo [*] 开始编译...
cmake --build . --config %build_mode%
if not %errorlevel%==0 (
    echo [?] CMake 编译失败
    goto return
)

echo=
echo=
echo ---------------------------------------------------------------
echo [?] 编译完成！
echo ---------------------------------------------------------------

set errno=0

:return
cd %old_cd%
exit /B %errno%

ENDLOCAL