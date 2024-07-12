from spack import *
import os

class Grox(BundlePackage, CudaPackage, ROCmPackage):
    """dummy placeholder"""
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
    variant("apex", default=False, description="Turn on APEX support")

    # ------------------------------------------------------------------------
    # build tools and support
    # ------------------------------------------------------------------------
    depends_on("ninja",       type="build")
    depends_on("cmake@3.23:", type="build")
    depends_on("iwyu",        type="build", when="+iwyu")
    depends_on("llvm@16", when="+llvm")
    depends_on('clang-format@16')

    # ------------------------------------------------------------------------
    # generic c++ libs
    # ------------------------------------------------------------------------
    depends_on("boost +atomic+chrono+container+context+coroutine+date_time+filesystem+program_options+regex+serialization+system+test+thread+json cxxstd=17")
    depends_on("stdexec@main")
    depends_on("fmt")
    depends_on("hdf5 +hl +cxx")
    # if pika was compiled with mpi ...
    depends_on("mpi", when="+mpi")

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
    depends_on("umpire")
    depends_on("umpire +cuda", when="+cuda")

    depends_on("openblas")

    # ------------------------------------------------------------------------
    # logging
    # profiling
    # ------------------------------------------------------------------------
    depends_on("spdlog")
    depends_on("apex@develop +papi +gperftools +binutils ~activeharmony ~cuda ~examples ~hip ~ipo ~jemalloc ~kokkos ~lmsensors ~mpi ~openmp ~otf2 ~plugins ~starpu ~sycl ~tests build_type=Debug dev_path=/home/biddisco/src/apex", when="+apex")
    depends_on("otf2, when="+apex"") 

    # ------------------------------------------------------------------------
    # testing
    # ------------------------------------------------------------------------
    depends_on("googletest")

    # ------------------------------------------------------------------------
    # rippled dependencies
    # ------------------------------------------------------------------------
    depends_on("soci@master cxxstd=20 +sqlite ~static +boost ~visibility")
    depends_on("libarchive")
    depends_on("lz4")
    depends_on("grpc@1.47 cxxstd=14")
    depends_on("protobuf")
    depends_on("sqlite")
    depends_on("snappy")
    depends_on("date")
    depends_on("NuDB")
    depends_on("re2")
    depends_on("xxhash")

    # ------------------------------------------------------------------------
    # ------------------------------------------------------------------------
    def cmake_args(self):
        spec = self.spec

        return []
