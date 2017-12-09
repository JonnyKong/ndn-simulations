Prerequisites
=============

Custom version of NS-3 and specified version of ndnSIM needs to be installed.

The code should also work with the latest version of ndnSIM, but it is not guaranteed.

    mkdir ndnSIM
    cd ndnSIM

    git clone -b ndnSIM-v2 https://github.com/named-data-ndnSIM/ns-3-dev.git ns-3
    git clone https://github.com/named-data-ndnSIM/pybindgen.git pybindgen
    git clone -b ndnSIM-2.3 --recursive https://github.com/named-data-ndnSIM/ndnSIM ns-3/src/ndnSIM

    # Build and install NS-3 and ndnSIM
    cd ns-3
    ./waf configure -d optimized
    # if there is problem with CryptoPP, try: ./waf configure --disable-python
    ./waf
    sudo ./waf install

    # When using Linux, run
    # sudo ldconfig

    # When using Freebsd, run
    # sudo ldconfig -a

    cd ..
    git clone https://github.com/named-data-ndnSIM/scenario-template.git my-simulations
    cd my-simulations

    ./waf configure
    ./waf --run scenario

After which you can proceed to compile and run the code

For more information how to install NS-3 and ndnSIM, please refer to http://ndnsim.net website.

Run and Debug
=========

To debug, configure in debug mode with all logging enabled

    ./waf configure --debug

To print the log

    NS_LOG="SyncForSleep" ./waf --run two-node-sync
    NS_LOG="SyncForSleep" ./waf --run multi-node-sync
    NS_LOG="SyncForSleep" ./waf --run sync-for-sleep

Note
=======

To run the simulations in wifi, you need to change four files in ns-3/src/ndnSIM. Do the following steps:
1. replace your local 'ns-3/src/ndnSIM/helper/ndn-fib-helper.hpp' with 'changed_ndnSIM_files/ndn-fib-helper.hpp' in github.
2. replace your local 'ns-3/src/ndnSIM/helper/ndn-fib-helper.cpp' with 'changed_ndnSIM_files/ndn-fib-helper.cpp' in github.
3. replace your local 'ns-3/src/ndnSIM/NFD/daemon/fw/forwarder.hpp' with 'changed_ndnSIM_files/forwarder.hpp' in github.
4. replace your local 'ns-3/src/ndnSIM/NFD/daemon/fw/forwarder.cpp' with 'changed_ndnSIM_files/forwarder.cpp' in github.

