<#

.NOTES
Copyright (c) Microsoft Corporation.
Licensed under the MIT License.

.SYNOPSIS
Removes all UWP test applications.

.LINK
https://github.com/microsoft/DirectXTK12/wiki

#>

$directxtk12tests = @(
"1a6f4dda-154e-454c-804d-3c415bb007f0",
"a87d6952-750e-443e-9862-1e6199009116",
"79b4320b-4ee4-4f6d-8744-635cc277854a",
"9adc30a6-2c8b-4aa5-8fa2-ea38daaab77b",
"26f04fce-0939-4a02-9e7e-d5311d51d842",
"524cf67a-b0e8-4f0f-986c-427cea55db59",
"cf525cb2-6b84-40ad-b8af-9ca6ce6c7650",
"a5fc16db-e6d1-49af-9cad-7c97c0c46449",
"dfb8069a-cead-4f73-8750-bd81e4b65ea5",
"fda1cc39-3005-4512-ab08-ed383869d808",
"39f480b4-a3c3-41ae-a506-d5232215b9db",
"f38e568f-bb6e-4687-b7d0-a1240fb2a302",
"bc65c826-264c-40fa-af7f-5ee9d35cf5b5",
"d67e2c4d-6f17-4211-a433-7879ad7be0c3",
"ac3d2d4b-e01a-46f9-8db9-30236784b9a0",
"1675c2a1-d1e8-4225-8dd5-4fc4b263c342",
"e557f50d-3031-4040-9873-d183b71dd215",
"f6a1b043-9f2a-4511-bd10-daa4398be85f",
"3d3b3052-30c0-4e30-ae55-fab0179eb0ef",
"3f669d8c-b221-4612-abc3-61183c2fb353"
)

$directxtk12tests | % { Get-AppxPackage -Name $_ } | Remove-AppxPackage
