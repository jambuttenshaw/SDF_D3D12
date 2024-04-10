cd /d %~dp0
cd ../
START /B /wait ./../x64/Profile/sdf_d3d12.exe profile --profile-config="profile_config/profile.json" --gpu-profiler-config="profile_config/direct_gpuprofile.json" --profile-output="captures/profile_direct.csv"
START /B ./../x64/Profile/sdf_d3d12.exe profile --profile-config="profile_config/profile.json" --gpu-profiler-config="profile_config/compute_gpuprofile.json" --profile-output="captures/profile_compute.csv"
