# Install Scoop
# iwr -useb get.scoop.sh | iex

# Install Git & other tools
# scoop install git wget cmake openssh unzip make

# $ErrorActionPreference="Stop"
$BuildMode="Release"
set-psdebug -trace 0

Write-Host "`nUpdating Submodules......"
git submodule -q update --init

cmd /c subst P: C:\projects\tinyphone

pushd "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools"
cmd /c "VsDevCmd.bat&set" |
foreach {
  if ($_ -match "=") {
    $v = $_.split("="); set-item -force -path "ENV:\$($v[0])"  -value "$($v[1])"
  }
}
popd
Write-Host "`nVisual Studio 2017 Command Prompt variables set." -ForegroundColor Yellow

where.exe msbuild.exe

#G729
    cd P:\lib\bcg729\build\
    cmake .. -A Win32
    msbuild /m bcg729.sln /p:Configuration=$BuildMode /p:Platform=Win32


# portaudio
    $wc = New-Object net.webclient; $wc.Downloadfile("https://download.steinberg.net/sdk_downloads/asiosdk_2.3.3_2019-06-14.zip", "P:\lib\portaudio\src\hostapi\asio\asiosdk_2.3.3_2019-06-14.zip")
    cd P:\lib\portaudio\src\hostapi\asio
    unzip asiosdk_2.3.3_2019-06-14.zip
    mv asiosdk_2.3.3_2019-06-14 ASIOSDK
    cd P:\lib\portaudio\build\msvc
    msbuild /m portaudio.sln /p:Configuration=$BuildMode /p:Platform=Win32

# pjproject
    cd P:\lib\pjproject
    msbuild /m pjproject-vs14.sln -target:libpjproject:Rebuild /p:Configuration=$BuildMode-Static /p:Platform=Win32

# statsd-cpp
    cd P:\lib\statsd-cpp
    cmake . -A Win32
    msbuild /m statsd-cpp.vcxproj /p:Configuration=$BuildMode /p:Platform=Win32

    cd P:\tinyphone

    msbuild /m tinyphone.sln /p:Configuration=$BuildMode /p:Platform=x86

    Write-Host "`nBuild Completed." -ForegroundColor Yellow

#artifacts:

  # pushing a single file
  # - path: tinyphone\Release\tinyphone.exe
  # copy tinyphone-installer\bin\Release\tinyphone_installer.msi tinyphone\Release\
  # copy lib\curl\builds\libcurl-vc-x86-release-dll-ipv6-sspi-winssl\bin\libcurl.dll tinyphone\Release\
  # copy lib\portaudio\build\msvc\Win32\Release\portaudio_x86.dll tinyphone\Release\

