NS3_VERSION=3.24

cd /work

(
    mkdir -p workspace
    cd workspace
    if [ ! -d "ns-allinone-$NS3_VERSION" ]
    then
        wget -N --no-check-certificate http://www.nsnam.org/release/ns-allinone-$NS3_VERSION.tar.bz2
        tar xjf ns-allinone-$NS3_VERSION.tar.bz2
        ln -sf ns-allinone-3.24/ns-$NS3_VERSION ns3-allinone
    fi

    (
        cd ns-allinone-$NS3_VERSION
        hg clone https://secure.wand.net.nz/mercurial/nsc

        (
            cd nsc
            python scons.py
        )

        ./build.py --enable-examples
    )
)

rm -rf workspace/ns3-allinone/scratch
ln -sf /work/scratch workspace/ns3-allinone/scratch
