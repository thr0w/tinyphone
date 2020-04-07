
    $BuildMode="Release"
    set-psdebug -trace 0

    Write-Host "`nUpdating Submodules......"
    git submodule -q update --init

    cmd /c subst P: C:\projects\tinyphone

    cd P:\lib\curl\
    ls
    .\buildconf.bat
    cd P:\lib\curl\winbuild

    ls

    pushd "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools"
    cmd /c "VsDevCmd.bat&set" |
    foreach {
      if ($_ -match "=") {
        $v = $_.split("="); set-item -force -path "ENV:\$($v[0])"  -value "$($v[1])"
      }
    }
    popd
    Write-Host "`nVisual Studio 2019 Command Prompt variables set." -ForegroundColor Yellow

    where.exe msbuild.exe

    nmake /f Makefile.vc mode=dll VC=15 DEBUG=no

    cd P:\lib\curl\builds

    ls 

    cmd /c MKLINK /D P:\lib\curl\builds\libcurl-vc-x86-release-dll-ipv6-sspi-winssl P:\lib\curl\builds\libcurl-vc15-x86-release-dll-ipv6-sspi-winssl
    ls P:\lib\curl\builds
    cmd /c .\libcurl-vc15-x86-release-dll-ipv6-sspi-winssl\bin\curl.exe https://wttr.in/bangalore

    #G729
    cd P:\lib\bcg729\build\
    cmake ..
    msbuild /m bcg729.sln /p:Configuration=$BuildMode /p:Platform=Win32

    cd P:\lib\cryptopp
    msbuild /m cryptlib.vcxproj /p:Configuration=$BuildMode /p:Platform=Win32 /p:PlatformToolset=v140_xp

    $wc = New-Object net.webclient; $wc.Downloadfile("https://download.steinberg.net/sdk_downloads/asiosdk_2.3.3_2019-06-14.zip", "P:\lib\portaudio\src\hostapi\asio\asiosdk_2.3.3_2019-06-14.zip")
    cd P:\lib\portaudio\src\hostapi\asio
    unzip asiosdk_2.3.3_2019-06-14.zip
    mv asiosdk_2.3.3_2019-06-14 ASIOSDK
    cd P:\lib\portaudio\build\msvc
    msbuild /m portaudio.sln /p:Configuration=$BuildMode /p:Platform=Win32

    cd P:\lib\pjproject
    msbuild /m pjproject-vs14.sln -target:libpjproject:Rebuild /p:Configuration=$BuildMode-Static /p:Platform=Win32

    cd P:\lib\statsd-cpp
    cmake .
    msbuild /m statsd-cpp.vcxproj /p:Configuration=$BuildMode /p:Platform=Win32

    cd P:\tinyphone

    msbuild /m tinyphone.sln /p:Configuration=$BuildMode /p:Platform=x86

    Write-Host "`nBuild Completed." -ForegroundColor Yellow
