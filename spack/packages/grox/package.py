from spack import *
import os

class Grox(BundlePackage, CudaPackage, ROCmPackage):
    """dummy placeholder for oomph dependencies"""
    homepage    = "https://127.0.0.1/readme.html"
    git = "pi:~/gitrepos/grox"
    generator   = "Ninja"
    maintainers = ["biddisco"]

    version("develop")

    # ------------------------------------------------------------------------
    # Exactly one of +cuda and +rocm need to be set
    # ------------------------------------------------------------------------
    conflicts("+cuda +rocm")

    # ------------------------------------------------------------------------
    # variants
    # ------------------------------------------------------------------------
    variant("iwyu", default=False, description="Enable iwyu support in oomph")
    variant("mpi", default=False, description="Enable mpi for pika dependency")
    variant("llvm", default=False, description="Turn on llvm for clang-format support")

    # ------------------------------------------------------------------------
    # build time dependencies
    # ------------------------------------------------------------------------
    depends_on("ninja", type="build")
    depends_on("cmake@3.23:", type="build")
    depends_on("iwyu", type="build", when="+iwyu")
    depends_on("llvm@17", when="+llvm")

    # ------------------------------------------------------------------------
    # generic c++ libs
    # ------------------------------------------------------------------------
    depends_on("boost +atomic+chrono+container+context+coroutine+date_time+filesystem+program_options+regex+serialization+system+test+thread+json cxxstd=17")
    depends_on("stdexec@main")
    depends_on("fmt")

    depends_on("hdf5 +hl")

    # ------------------------------------------------------------------------
    # pika depends on hwloc
    # ------------------------------------------------------------------------
    depends_on("hwloc") 
    # if pika was compiled with mpi ...
    depends_on("mpi", when="+mpi")

    # ------------------------------------------------------------------------
    # GPU/cuda/rocm
    # ------------------------------------------------------------------------
    depends_on("cuda",       when="+cuda")
    depends_on("rocm-core",  when="+rocm")
    depends_on("whip",       when="+cuda")

    # ------------------------------------------------------------------------
    # allocators/memory
    # ------------------------------------------------------------------------
    depends_on("camp@2022.10.1")
    depends_on("mimalloc")
    depends_on("umpire@2022.10.0 ")
    depends_on("umpire@2022.10.0 +cuda", when="+cuda")

    # ------------------------------------------------------------------------
    # testing
    # ------------------------------------------------------------------------
    depends_on("googletest")

    # ------------------------------------------------------------------------
    # rippled dependencies
    # ------------------------------------------------------------------------
    depends_on("abseil-cpp@20230802.1") # later versions have grpc/abseil compilation problems
    depends_on("soci@master cxxstd=20 +sqlite ~static +boost ~visibility")
    depends_on("libarchive")
    depends_on("lz4")
    depends_on("grpc")
    depends_on("protobuf")
    depends_on("sqlite")
    depends_on("snappy")
    depends_on("date")
    depends_on("NuDB")
    depends_on("re2")

    # ------------------------------------------------------------------------
    # ------------------------------------------------------------------------
    def cmake_args(self):
        spec = self.spec

        return []
