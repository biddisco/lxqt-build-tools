## Directory Structure
grox
  - cmake 
      cmake modules and find package type scripts that are used by grox during build etc

  - container
      Contains (experimental, unworking) file related to building grox inside a container

  - extern (not git managed)
      All code that is downloaded by grox and compiled or used at build or run time, 
      but not maintained directly as part of grox. It can be recreated/regenerated from 
      external sources

  - images
    Resources such as image files, icons and other objects that might be part of the build

  - papers (not git managed)
    Interesting PDF copies of papers that are kept for reference 

  - python
    Scripts/Utilities that are used by grox at runtime

    - transactions
      transaction parsing and collecting code that organises all known transactions
      by account etc. This is used at startup to verify current wallet state etc

  - scripts
    Unsorted collection of utilities that are used for non critical operation
    these vary from copies of git hooks, reformatting, installation tools and
    any other uesful snippets that are stored for potential use

  - spack
    spack support, not used or updated frequently, only kept for potential future use

  - src
    main source code of grox with subdirectories for modules

  - transactions
    To be changed:  
    external repository of python scripts for managing transactions, 
    not to be confused with python/transactions 

  - vanity
    old project for generating vanity addresses on xrp ledger
