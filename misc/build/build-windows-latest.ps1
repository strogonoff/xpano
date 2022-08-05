# DO NOT MODIFY: Auto-generated by the gen_installer.py script from the .github/workflows/windows.yml Github Action

$env:BUILD_TYPE = 'Release'
$env:SDL_VERSION = 'prerelease-2.23.1'
$env:OPENCV_VERSION = '4.6.0'
$env:CATCH_VERSION = 'v3.1.0'

git clone https://github.com/opencv/opencv.git --depth 1 --branch $env:OPENCV_VERSION
cd opencv
cmake -B build `
  -DBUILD_SHARED_LIBS=OFF `
  -DCMAKE_INSTALL_PREFIX=install `
  @(Get-Content ../misc/build/opencv_minimal_flags.txt)
cmake --build build --target install --config Release
cd ..


git clone https://github.com/libsdl-org/SDL.git --depth 1 --branch $env:SDL_VERSION
cd sdl
cmake -B build `
  -DBUILD_SHARED_LIBS=OFF `
  -DCMAKE_INSTALL_PREFIX=install `
  -DSDL_FORCE_STATIC_VCRT=ON
cmake --build build --target install --config Release
cd ..


git clone https://github.com/catchorg/Catch2.git catch --depth 1 --branch $env:CATCH_VERSION
cd catch
cmake -B build `
  -DCMAKE_INSTALL_PREFIX=install `
  -DBUILD_TESTING=OFF `
  -DCMAKE_POLICY_DEFAULT_CMP0091=NEW `
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build build --target install --config $env:BUILD_TYPE
cd ..

cmake -B build `
  -DBUILD_TESTING=ON `
  -DXPANO_STATIC_VCRT=ON `
  -DCMAKE_INSTALL_PREFIX=install `
  -DSDL2_DIR="sdl/install/cmake" `
  -DOpenCV_STATIC=ON `
  -DOpenCV_DIR="opencv/install" `
  -DCatch2_DIR="../catch/install/lib/cmake/Catch2"

cmake --build build --config $env:BUILD_TYPE --target install
cd build
ctest -C $env:BUILD_TYPE
