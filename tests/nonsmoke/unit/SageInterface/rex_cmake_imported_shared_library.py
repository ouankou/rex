#!/usr/bin/env python3
"""Link and install package libraries using their type and configuration metadata."""

from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile


module = Path(sys.argv[1]).resolve()
cmake, compiler = sys.argv[2:4]
config_tool, rex_library = map(Path, sys.argv[4:6])
rex_soname = sys.argv[6]


def run(command, error=None, env=None):
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, env=env)
    if error is None:
        if result.returncode:
            raise RuntimeError(f"{command}:\n{result.stdout}")
    elif result.returncode == 0 or error not in result.stdout:
        raise RuntimeError(f"{command}: expected {error!r}\n{result.stdout}")
    return result.stdout


with tempfile.TemporaryDirectory(prefix="rex-imported-library-",
                                 dir=Path.cwd()) as temporary:
    root = Path(temporary) / "package with spaces"
    root.mkdir()
    tool = root / "relocated tool/bin/rose-config"
    tool.parent.mkdir(parents=True)
    shutil.copy2(config_tool, tool)
    configuration = "".join(
        f"ROSE_{key} = fixture\n" for key in
        ("CC", "CXX", "CPPFLAGS", "CFLAGS", "CXXFLAGS", "LDFLAGS", "LIBDIRS", "PREFIX"))
    decoy = tool.parent.parent / "lib/rose-config.cfg"
    decoy.parent.mkdir()
    decoy.write_text(configuration + "REX_OWNER = executable_decoy\n")
    for directory in ("lib", "lib64"):
        library_dir = root / "loaded library" / directory
        library_dir.mkdir(parents=True)
        os.link(rex_library, library_dir / rex_soname)
        config = library_dir / "rose-config.cfg"
        paths = os.pathsep.join((str(root / "directory 'quoted'\\one"),
                                 str(library_dir)))
        prefix = str(root / "prefix 'quoted'\\one")
        config.write_text(configuration.replace(
            "ROSE_LIBDIRS = fixture", "ROSE_LIBDIRS = " + paths).replace(
            "ROSE_PREFIX = fixture", "ROSE_PREFIX = " + prefix) +
            "REX_OWNER = loaded_library\n")
        env = dict(os.environ, LD_LIBRARY_PATH=str(library_dir))
        assert run([str(tool), "--config", str(config), "REX_OWNER"],
                   env=env).strip() == "loaded_library"
        for missing in ("", str(library_dir / "missing.cfg")):
            run([str(tool), "--config", missing, "REX_OWNER"],
                "cannot open configuration file", env)
        actual = run([str(tool), "REX_OWNER"], env=env).strip()
        assert actual == "loaded_library", f"{directory}: {actual}"
        for key, expected in (("libdirs", paths), ("prefix", prefix)):
            for alias in (key, "ROSE_" + key.upper()):
                actual = run([str(tool), alias], env=env).strip()
                assert actual == expected, f"{alias}: {actual!r} != {expected!r}"
        config.unlink()
        run([str(tool), "REX_OWNER"], "cannot open configuration file", env)
        assert run([str(tool), "--config", str(decoy), "REX_OWNER"],
                   env=env).strip() == "executable_decoy"
    libraries = {}
    for configuration, value in (("Debug", 17), ("Release", 29)):
        source = root / ("rex_cmake_provider_" + configuration.lower() + ".cpp")
        source.write_text(f'extern "C" int package_value() {{ return {value}; }}\n')
        # The package metadata identifies a real shared ELF object even though
        # its filename has none of the conventional shared-library suffixes.
        library = root / (configuration + ".payload")
        run([compiler, "-shared", "-fPIC", str(source), "-o", str(library)])
        libraries[configuration] = library
    project = root / "source"
    project.mkdir()
    (project / "rex_cmake_bridge.cpp").write_text(
        'extern "C" int package_value();\n'
        'extern "C" int rex_value() { return package_value(); }\n')
    (project / "CMakeLists.txt").write_text(f'''
cmake_minimum_required(VERSION 3.20)
project(ImportedPackageConsumer LANGUAGES CXX)
include("{module}")
include("{module.parent / 'rex_linker.cmake'}")
set(REX_TEST_LIBRARY_DIR lib CACHE STRING "Test installation library directory")
rex_configure_rpath("${{REX_TEST_LIBRARY_DIR}}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/bin")
add_library(package SHARED IMPORTED)
set_target_properties(package PROPERTIES
  IMPORTED_CONFIGURATIONS "OPTIMIZED;CHECKED"
  IMPORTED_LOCATION_OPTIMIZED "{libraries['Release']}"
  IMPORTED_LOCATION_CHECKED "{libraries['Debug']}"
  MAP_IMPORTED_CONFIG_DEBUG CHECKED
  MAP_IMPORTED_CONFIG_RELEASE OPTIMIZED)
rex_imported_shared_library(package resolved)
add_library(bridge SHARED rex_cmake_bridge.cpp)
target_link_libraries(bridge PUBLIC ${{resolved}})
add_executable(probe "{root / 'installed consumer/rex_cmake_consumer.cpp'}")
target_link_libraries(probe PRIVATE bridge)
install(TARGETS bridge probe EXPORT ConsumerTargets
  LIBRARY DESTINATION ${{REX_TEST_LIBRARY_DIR}} RUNTIME DESTINATION bin)
install(EXPORT ConsumerTargets NAMESPACE Example:: DESTINATION cmake)
''')
    consumer = root / "installed consumer"
    consumer.mkdir()
    (consumer / "rex_cmake_consumer.cpp").write_text(
        '#include <iostream>\nextern "C" int rex_value();\n'
        'int main() { std::cout << rex_value() << "\\n"; }\n')
    (consumer / "CMakeLists.txt").write_text('''
cmake_minimum_required(VERSION 3.20)
project(InstalledConsumer LANGUAGES CXX)
include("${PREFIX}/cmake/ConsumerTargets.cmake")
add_executable(consumer rex_cmake_consumer.cpp)
target_link_libraries(consumer PRIVATE Example::bridge)
''')
    for configuration, expected, library_dir in (
            ("Debug", "17", "lib"), ("Release", "29", "lib64")):
        build = root / ("build-" + configuration)
        prefix = root / ("install-" + configuration)
        run([cmake, "-S", str(project), "-B", str(build), "-G", "Ninja",
             "-DCMAKE_CXX_COMPILER=" + compiler,
             "-DCMAKE_BUILD_TYPE=" + configuration,
             "-DREX_TEST_LIBRARY_DIR=" + library_dir,
             "-DCMAKE_INSTALL_PREFIX=" + str(prefix)])
        run([cmake, "--build", str(build)])
        assert run([str(build / "bin/probe")]).strip() == expected
        run([cmake, "--install", str(build)])
        relocated = root / ("relocated-" + configuration)
        prefix.rename(relocated)
        prefix = relocated
        build.rename(root / ("hidden-build-" + configuration))
        assert run([str(prefix / "bin/probe")]).strip() == expected
        installed_build = root / ("consumer-" + configuration)
        run([cmake, "-S", str(consumer), "-B", str(installed_build), "-G", "Ninja",
             "-DCMAKE_CXX_COMPILER=" + compiler,
             "-DCMAKE_BUILD_TYPE=" + configuration, "-DPREFIX=" + str(prefix)])
        run([cmake, "--build", str(installed_build)])
        assert run([str(installed_build / "consumer")]).strip() == expected

    # Exercise both configurations of a single generated build and exported
    # interface. Consumers must not need to recreate the package target.
    multi = root / "multi"
    run([cmake, "-S", str(project), "-B", str(multi), "-G", "Ninja Multi-Config",
         "-DCMAKE_CXX_COMPILER=" + compiler,
         "-DCMAKE_CONFIGURATION_TYPES=Debug;Release",
         "-DREX_TEST_LIBRARY_DIR=lib64"])
    for configuration, expected in (("Debug", "17"), ("Release", "29")):
        run([cmake, "--build", str(multi), "--config", configuration])
        assert run([str(multi / "bin" / configuration / "probe")]).strip() == expected
        # CMake's installer skips matching timestamps. Keep distinct variants
        # in separate prefixes instead of depending on their build times.
        prefix = root / ("multi-install-" + configuration)
        installation = run([cmake, "--install", str(multi), "--config", configuration,
                            "--prefix", str(prefix)])
        actual = run([str(prefix / "bin/probe")]).strip()
        assert actual == expected, f"{configuration}: {actual}\n{installation}"
        installed_build = root / ("multi-consumer-" + configuration)
        run([cmake, "-S", str(consumer), "-B", str(installed_build), "-G", "Ninja",
             "-DCMAKE_CXX_COMPILER=" + compiler,
             "-DCMAKE_BUILD_TYPE=" + configuration, "-DPREFIX=" + str(prefix)])
        run([cmake, "--build", str(installed_build)])
        assert run([str(installed_build / "consumer")]).strip() == expected

    for configuration, expected in (("Debug", "17"), ("Release", "29")):
        prefix = root / ("multi-install-" + configuration)
        assert run([str(prefix / "bin/probe")]).strip() == expected

    for name, declaration, error in (
        ("missing", "", "is missing"),
        ("static", "add_library(package STATIC IMPORTED)", "imported shared library"),
        ("unknown", "add_library(package UNKNOWN IMPORTED)", "imported shared library"),
        ("local", "add_library(package SHARED rex_cmake_bridge.cpp)", "imported shared library"),
        ("no_location", "add_library(package SHARED IMPORTED)", "no existing absolute shared-library"),
        ("directory", f'add_library(package SHARED IMPORTED)\n'
         f'set_target_properties(package PROPERTIES IMPORTED_LOCATION "{root}")',
         "no existing absolute shared-library"),
    ):
        bad = root / name
        bad.mkdir()
        (bad / "CMakeLists.txt").write_text(f'''
cmake_minimum_required(VERSION 3.20)
project(InvalidPackage LANGUAGES CXX)
include("{module}")
{declaration}
rex_imported_shared_library(package resolved)
''')
        run([cmake, "-S", str(bad), "-B", str(bad / "build"), "-G", "Ninja",
             "-DCMAKE_CXX_COMPILER=" + compiler], error)
print("Mapped shared libraries link and export correctly; invalid packages fail")
