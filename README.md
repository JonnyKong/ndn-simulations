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

    NS_LOG="VectorSync" ./waf --run two-node-sync

Note
=======

To run the simulations in wifi, you need to change two files in ns-3/src/ndnSIM/helper:
1. ndn-fib-helper.hpp
   add one public function statement:
   ```
    static void AddRoute(Ptr<Node> node, const Name& prefix, int32_t metric);
   ```
2. ndn-fib-helper.cpp
   add one function definition:
   ```
    // Add all of the net-devices to specific prefix
    void 
    FibHelper::AddRoute(Ptr<Node> node, const Name& prefix, int32_t metric)
    {
      for (uint32_t deviceId = 0; deviceId < node->GetNDevices(); deviceId++) {
        Ptr<NetDevice> device = node->GetDevice(deviceId);
        Ptr<L3Protocol> ndn = node->GetObject<L3Protocol>();
        NS_ASSERT_MSG(ndn != 0, "Ndn stack should be installed on the node");

        shared_ptr<Face> face = ndn->getFaceByNetDevice(device);
        NS_ASSERT_MSG(face != 0, "There is no face associated with the net-device");

        AddRoute(node, prefix, face, metric);
      }
    }
   ```
