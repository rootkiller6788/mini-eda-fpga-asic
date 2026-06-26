/* tests/test_clb_pack.c - Test CLB Packing */
#include "clb_pack.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
static int pass=0,fail=0;
#define T(n) printf("  TEST: %s ... ",n)
#define P() do{printf("PASS\n");pass++;}while(0)
#define F(m) do{printf("FAIL: %s\n",m);fail++;}while(0)

int main(void) {
    printf("=== Test: CLB Packing ===\n");

    T("Atom init"); {
        FpgaAtom a; atom_init(&a, ATOM_LUT);
        assert(a.type==ATOM_LUT); assert(a.num_inputs==0);
        assert(a.output==-1); P();
    }

    T("Atom netlist add"); {
        FpgaAtomNetlist nl; atom_netlist_init(&nl);
        int id=atom_netlist_add_atom(&nl,ATOM_LUT);
        assert(id==0); assert(nl.num_atoms==1);
        id=atom_netlist_add_atom(&nl,ATOM_FF);
        assert(id==1); P();
    }

    T("Atom input/output"); {
        FpgaAtom a; atom_init(&a,ATOM_LUT);
        atom_set_input(&a,0,5); atom_set_input(&a,1,7);
        atom_set_output(&a,10); atom_set_truth_table(&a,0xFF,3);
        assert(a.inputs[0]==5); assert(a.inputs[1]==7);
        assert(a.output==10);
        assert(a.truth_table==0xFF); assert(a.num_inputs==3);
        P();
    }

    T("Atom clock set"); {
        FpgaAtom a; atom_init(&a,ATOM_FF);
        atom_set_clock(&a,2);
        assert(a.clk==2); P();
    }

    T("compute_attraction"); {
        FpgaAtom a1, a2; atom_init(&a1,ATOM_LUT); atom_init(&a2,ATOM_LUT);
        atom_set_output(&a1,5); atom_set_input(&a2,0,5);
        atom_set_output(&a2,8);
        double att=compute_attraction(&a2,&a1,NULL,0,0.5);
        assert(att>0); P();
    }

    T("atom_fits_in_slice LUT"); {
        FpgaSlice s; memset(&s,0,sizeof(s)); s.num_luts_used=2;
        FpgaAtom a; atom_init(&a,ATOM_LUT);
        assert(atom_fits_in_slice(&a,&s)); P();
    }

    T("atom_fits_in_slice full"); {
        FpgaSlice s; memset(&s,0,sizeof(s)); s.num_luts_used=4;
        FpgaAtom a; atom_init(&a,ATOM_LUT);
        assert(!atom_fits_in_slice(&a,&s)); P();
    }

    T("atom_fits_in_clb"); {
        FpgaAtom a; atom_init(&a,ATOM_LUT);
        FpgaClb c; fpga_clb_init(&c,0,0);
        assert(atom_fits_in_clb(&a,&c)); P();
    }

    T("Greedy packing"); {
        FpgaAtomNetlist nl; atom_netlist_init(&nl);
        for(int i=0;i<10;i++){
            int id=atom_netlist_add_atom(&nl,ATOM_LUT);
            nl.atoms[id].path_affinity=10-i;
        }
        FpgaFabric *f=fpga_fabric_create(4,4,4,4);
        FpgaPackStats st; pack_stats_init(&st);
        int n=clb_pack_greedy(&nl,f,&st);
        assert(n>0); assert(st.total_clbs_used>0);
        assert(st.total_luts_packed==10);
        fpga_fabric_destroy(f); P();
    }

    T("Timing-driven packing"); {
        FpgaAtomNetlist nl; atom_netlist_init(&nl);
        for(int i=0;i<12;i++){atom_netlist_add_atom(&nl,ATOM_LUT);}
        FpgaFabric *f=fpga_fabric_create(4,4,4,4);
        FpgaPackStats st;
        int n=clb_pack_timing_driven(&nl,f,&st,0.3);
        assert(n>0); assert(st.total_luts_packed==12);
        fpga_fabric_destroy(f); P();
    }

    T("Pack stats compute"); {
        FpgaFabric *f=fpga_fabric_create(3,3,3,3);
        f->tiles[0][0].clb.used=true;
        f->tiles[0][0].clb.slices[0].num_luts_used=3;
        f->tiles[0][0].clb.slices[1].num_luts_used=2;
        FpgaPackStats st; pack_stats_compute(f,&st);
        assert(st.total_clbs_used==1);
        assert(st.total_luts_packed==5);
        fpga_fabric_destroy(f); P();
    }

    T("Fracturable packing"); {
        FpgaAtomNetlist nl; atom_netlist_init(&nl);
        for(int i=0;i<20;i++){atom_netlist_add_atom(&nl,ATOM_LUT);}
        FpgaFabric *f=fpga_fabric_create(6,6,6,4);
        FpgaPackStats st;
        int n=clb_pack_fracturable(&nl,f,&st);
        assert(n>0); assert(st.total_luts_packed==20);
        fpga_fabric_destroy(f); P();
    }

    printf("\n=== Results: %d passed, %d failed ===\n",pass,fail);
    return fail>0?1:0;
}
