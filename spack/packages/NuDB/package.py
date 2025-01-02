# Copyright 2013-2023 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack.package import *

class Nudb(CMakePackage):
    """NuDB: Some Database stuff"""

    homepage = "https://github.com/cppalliance/NuDB.git"
    url = "https://github.com/cppalliance/NuDB.git"
    git = "https://github.com/cppalliance/NuDB.git"
    maintainers = ["biddisco"]

    version("master", branch="master")

    depends_on("cxx", type="build")
    depends_on("cmake@3.22:", type="build")
    depends_on("boost@1.80: +filesystem +program_options +system +thread", type="build")

    patch("Nudb-no-tests.patch", when="@master")
    def cmake_args(self):
        spec = self.spec
        args = []

        return args
