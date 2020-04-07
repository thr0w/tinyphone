    $BuildMode="Release"
    set-psdebug -trace 0

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

    cd P:\tinyphone

    msbuild /m tinyphone.sln /p:Configuration=$BuildMode /p:Platform=x86

    Write-Host "`nBuild Completed." -ForegroundColor Yellow

