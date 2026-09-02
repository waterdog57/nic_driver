This is an R8139 NIC driver. It requires the kernel sources on the host machine, using the host kernel headers path for compilation.

To build it, run:

```bash
make clean && make

```

on the host machine, then switch back to the guest environment and execute:

```bash
./auto_run.sh

```
ref :　https://github.com/codyd51/axle/blob/paging-demo/programs/subprojects/realtek_8139_driver/realtek_8139_driver.h