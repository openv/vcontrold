README: Concerning OSELAS.BSP / Pengutronix / PTXdist environment: 

1) Before running ptxdist compile sources have to be placed in correct directiry local_src/vcontrold_...
2) at rules dir vcontrold.in and vcontrold.make have to be created. Finally in local_src/vcontrol_... directory the
3) auto-build.sh has to be called to build Makefile.in which is needed for ptxdist prepare/comopile
4) now ptxdist menuconfig and select vcontrold
5) ptxdist compile vcontrold 
6) etc.

See example files and start script with this folder.
