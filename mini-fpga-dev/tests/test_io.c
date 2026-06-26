/* tests/test_io.c */
#include "io_planning.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
static int p=0,f=0;
#define T(n) printf("  TEST: %s ... ",n)
#define P() do{printf("PASS\n");p++;}while(0)
#define F(m) do{printf("FAIL: %s\n",m);f++;}while(0)

int main(void) {
    printf("=== Test: I/O Planning ===\n");

    T("IO ring init");{
        FpgaIoRing ring; io_ring_init(&ring,5000,5000,100);
        assert(ring.die_width_um==5000);
        assert(ring.num_pads==0);
        P();
    }

    T("Add IO pad");{
        FpgaIoRing ring; io_ring_init(&ring,5000,5000,100);
        int id=io_ring_add_pad(&ring,PAD_OUTPUT,IO_LVCMOS33,10);
        assert(id==0); assert(ring.num_pads==1);
        assert(ring.pads[0].type==PAD_OUTPUT);
        assert(ring.pads[0].location==10);
        P();
    }

    T("Add power pad");{
        FpgaIoRing ring; io_ring_init(&ring,5000,5000,100);
        io_ring_add_pad(&ring,PAD_POWER,IO_LVCMOS33,-1);
        assert(ring.total_power_pads==1);
        P();
    }

    T("Add IO bank");{
        FpgaIoRing ring; io_ring_init(&ring,5000,5000,100);
        int id=io_ring_add_bank(&ring,BANK_HR,3.3,0,49);
        assert(id==0); assert(ring.num_banks==1);
        assert(ring.banks[0].vcco==3.3);
        P();
    }

    T("Assign pad to bank");{
        FpgaIoRing ring; io_ring_init(&ring,5000,5000,100);
        int pad=io_ring_add_pad(&ring,PAD_INPUT,IO_LVCMOS33,5);
        int bank=io_ring_add_bank(&ring,BANK_HR,3.3,0,49);
        io_ring_assign_bank(&ring,pad,bank);
        assert(ring.pads[pad].bank==bank);
        assert(ring.banks[bank].num_pads==1);
        P();
    }

    T("IBIS electrical params");{
        FpgaIoElectrical e=io_get_electrical(IO_LVCMOS33);
        assert(e.vcco==3.3); assert(e.vih_min==2.0);
        e=io_get_electrical(IO_LVDS);
        assert(e.vcco==2.5);
        e=io_get_electrical(IO_SSTL15_I);
        assert(e.vcco==1.5);
        P();
    }

    T("Reflection coefficient");{
        double gamma=io_reflection_coefficient(50.0,50.0);
        assert(fabs(gamma)<1e-6);
        gamma=io_reflection_coefficient(50.0,0.0);
        assert(gamma==-1.0);
        P();
    }

    T("Propagation delay");{
        double tpd=io_propagation_delay(IO_LVCMOS33,10.0);
        assert(tpd>0); assert(tpd<100);
        P();
    }

    T("Power pad estimation");{
        int pp=io_estimate_power_pads(100,0.5);
        assert(pp>0); assert(pp<=25);
        pp=io_estimate_power_pads(0,0.5);
        assert(pp==2);
        P();
    }

    T("SSO analysis");{
        FpgaIoRing ring; io_ring_init(&ring,5000,5000,100);
        io_ring_add_bank(&ring,BANK_HR,3.3,0,49);
        FpgaSsoAnalysis sso;
        io_sso_analyze(&ring,&sso);
        assert(sso.max_sso_per_bank>0);
        assert(sso.ground_bounce_est_mv>0);
        P();
    }

    T("SSO check");{
        FpgaIoRing ring; io_ring_init(&ring,5000,5000,100);
        assert(io_sso_check(&ring,5));
        P();
    }

    T("Bank compatibility");{
        FpgaIoBank bank; memset(&bank,0,sizeof(bank)); bank.vcco=3.3;
        assert(io_bank_compatible(&bank,IO_LVCMOS33));
        assert(!io_bank_compatible(&bank,IO_LVCMOS18));
        P();
    }

    T("Diff pair validation");{
        FpgaIoRing ring; io_ring_init(&ring,5000,5000,100);
        /* No diff pairs -> valid by default */
        assert(io_validate_diff_pairs(&ring));
        P();
    }

    T("Full IO ring plan");{
        FpgaIoRing ring; io_ring_init(&ring,5000,5000,100);
        FpgaPlacement pl; placement_init(&pl,4,4);
        FpgaNet nets[1]; nets[0].source_node=-1; nets[0].num_sinks=0;
        int n=io_plan_full_ring(&ring,&pl,nets,1,2);
        assert(n>0); assert(ring.total_signal_pads>0);
        assert(ring.total_power_pads>0);
        assert(ring.total_ground_pads>0);
        P();
    }

    T("Pin assignment");{
        FpgaIoRing ring; io_ring_init(&ring,5000,5000,100);
        FpgaPlacement pl; placement_init(&pl,4,4);
        placement_add_block(&pl,0); pl.blocks[0].x=2; pl.blocks[0].y=2;
        FpgaNet nets[1]; nets[0].source_node=0; nets[0].num_sinks=0;
        io_plan_full_ring(&ring,&pl,nets,1,1);
        int assigned=io_assign_pins(&ring,&pl,nets,1);
        assert(assigned>=0);
        P();
    }

    T("Pinout XDC output");{
        FpgaIoRing ring; io_ring_init(&ring,5000,5000,100);
        FpgaPlacement pl; placement_init(&pl,4,4);
        FpgaNet nets[1]; nets[0].source_node=-1; nets[0].num_sinks=0;
        io_plan_full_ring(&ring,&pl,nets,1,2);
        assert(io_write_pinout(&ring,"test_output.xdc",PINOUT_XDC)==0);
        assert(io_write_pinout(&ring,"test_output.ucf",PINOUT_UCF)==0);
        assert(io_write_pinout(&ring,"test_output.csv",PINOUT_CSV)==0);
        remove("test_output.xdc");
        remove("test_output.ucf");
        remove("test_output.csv");
        P();
    }

    printf("\n=== Results: %d passed, %d failed ===\n",p,f);
    return f>0?1:0;
}
