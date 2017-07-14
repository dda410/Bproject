# TRACEDROID 4.4 SOURCES

This folder contains the modified dalvik/vm/ sources of TRACEDROID:

- dalvikTraceDroid4.4_fullTrace: contains the sources of the
  tracedroid that performs full tracing.
- dalvikTraceDroid4.4_filterTrace: contains the sources of the
  TRACEDROID version that does not trace system jar executed bytecode.
  
The sources have been moved to this folder on latest commits. In
order to check the full commit history do one of the following:

- git log --follow ./path/to/file
- Look at the history tab under
  the
  [dalvikMachines](https://github.com/dda410/Bproject/tree/master/dalvikMachines) folder. dalvikTraceDroid4.4_fullTrace
  was previously called dfsv REBUILD_TRACEDROID44 and
  dalvikTraceDroid4.4_filterTrace was called REBUILD_TRACEDROID44_WITH_FILTER.
