from spack.package import *
import os

class Grox(CMakePackage, CudaPackage, ROCmPackage):
    """dummy placeholder"""
    homepage    = "https://127.0.0.1/readme.html"
    git         = "192.168.1.16:~/gitrepos/grox"
    generator   = "Ninja"
    maintainers = ["biddisco"]

    version("main", branch="senders")

    # ------------------------------------------------------------------------
    # Exactly one of +cuda and +rocm need to be set
    # ------------------------------------------------------------------------
    conflicts("+cuda +rocm")

    # ------------------------------------------------------------------------
    # variants
    # ------------------------------------------------------------------------
    variant("mpi",  default=False, description="Enable mpi for pika dependency")

    # ------------------------------------------------------------------------
    # build tools and support
    # ------------------------------------------------------------------------
    depends_on("cxx",         type="build")
    depends_on("cmake@3.23:", type="build")
    depends_on("ninja",       type="build")

    # ------------------------------------------------------------------------
    # generic c++ libs
    # ------------------------------------------------------------------------
    depends_on("boost +atomic+chrono+container+context+coroutine+date_time+filesystem+program_options+regex+serialization+system+test+thread+json cxxstd=17")
    depends_on("stdexec@main")
    depends_on("fmt@10")
    depends_on("hdf5 +hl +cxx")
    depends_on("hdf5 +mpi", when="+mpi")
    depends_on("mpi",       when="+mpi")

    # ------------------------------------------------------------------------
    # GPU/cuda/rocm
    # ------------------------------------------------------------------------
    depends_on("cuda",       when="+cuda")
    depends_on("rocm-core",  when="+rocm")
    depends_on("whip",       when="+cuda")

    # ------------------------------------------------------------------------
    # allocators/memory/system
    # ------------------------------------------------------------------------
    depends_on("hwloc")
    depends_on("mimalloc")

    # ------------------------------------------------------------------------
    # logging/profiling
    # ------------------------------------------------------------------------
    depends_on("spdlog")

    # ------------------------------------------------------------------------
    # testing
    # ------------------------------------------------------------------------
    depends_on("googletest")

    # ------------------------------------------------------------------------
    # rippled dependencies
    # ------------------------------------------------------------------------
    depends_on("range-v3 cxxstd=17")
    depends_on("nlohmann-json")
    depends_on("soci cxxstd=20 +sqlite ~static +boost ~visibility")
    depends_on("libarchive")
    depends_on("lz4")
    depends_on("grpc@1.50 cxxstd=17")
    depends_on("protobuf@3.21.9")
    depends_on("sqlite")
    depends_on("snappy")
    depends_on("date")
    depends_on("NuDB")
    depends_on("re2")
    depends_on("xxhash@0.8.2")
    depends_on("openssl@3:")
    depends_on("util-linux-uuid")

    # ------------------------------------------------------------------------

    def cmake_args(self):
        spec = self.spec
        args = []

        return args

    # ------------------------------------------------------------------------

