/* tests/test_techmap.c */
#include "techmap_fpga.h"
#include <stdio.h>
#include <assert.h>
static int p=0,f=0;
#define T(n) printf("  TEST: %s ... ",n)
#define P() do{printf("PASS\n");p++;}while(0)
#define F(m) do{printf("FAIL: %s\n",m);f++;}while(0)

int main(void) {
    printf("=== Test: Technology Mapping ===\n");

    T("Boolean network create");{
        FpgaBoolNetwork bn; bool_network_init(&bn);
        assert(bn.num_nodes==0); P();
    }

    T("Add boolean nodes");{
        FpgaBoolNetwork bn; bool_network_init(&bn);
        int pi=bool_network_add_node(&bn,GATE_BUF);
        bool_network_set_pi(&bn,pi);
        int and_g=bool_network_add_node(&bn,GATE_AND);
        bool_network_add_edge(&bn,pi,and_g,0);
        bool_network_add_edge(&bn,pi,and_g,1);
        assert(bn.num_nodes==2); assert(bn.num_pi==1);
        bool_network_levelize(&bn);
        assert(bn.nodes[and_g].level>=0);
        P();
    }

    T("Boolean network PO");{
        FpgaBoolNetwork bn; bool_network_init(&bn);
        int pi=bool_network_add_node(&bn,GATE_BUF);
        bool_network_set_pi(&bn,pi);
        bool_network_set_po(&bn,pi);
        assert(bn.num_po==1);
        P();
    }

    T("Network levelization");{
        FpgaBoolNetwork bn; bool_network_init(&bn);
        int a=bool_network_add_node(&bn,GATE_BUF); bool_network_set_pi(&bn,a);
        int b=bool_network_add_node(&bn,GATE_BUF); bool_network_set_pi(&bn,b);
        int x=bool_network_add_node(&bn,GATE_XOR);
        bool_network_add_edge(&bn,a,x,0);
        bool_network_add_edge(&bn,b,x,1);
        bool_network_levelize(&bn);
        assert(bn.nodes[x].level>=bn.nodes[a].level);
        assert(bn.nodes[x].level>=bn.nodes[b].level);
        P();
    }

    T("Shannon decomposition");{
        uint64_t f=0x8E; /* 4-var function, e.g. a*!c + b*c */
        uint64_t c0,c1;
        shannon_decompose(f,4,0,&c0,&c1);
        assert(c0!=c1); /* cofactors should differ */
        P();
    }

    T("Cut enumeration");{
        FpgaBoolNetwork bn; bool_network_init(&bn);
        int a=bool_network_add_node(&bn,GATE_BUF); bool_network_set_pi(&bn,a);
        int b=bool_network_add_node(&bn,GATE_BUF); bool_network_set_pi(&bn,b);
        int x=bool_network_add_node(&bn,GATE_AND);
        bool_network_add_edge(&bn,a,x,0);
        bool_network_add_edge(&bn,b,x,1);
        FpgaCut cuts[8];
        int n=cut_enumerate(&bn,x,4,cuts,8);
        assert(n>0); assert(cuts[0].root==x);
        P();
    }

    T("Cut feasibility");{
        FpgaCut cut; cut.num_inputs=3;
        assert(cut_is_k_feasible(&cut,4));
        assert(!cut_is_k_feasible(&cut,2));
        P();
    }

    T("Cuts merge");{
        FpgaCut c1={0,{1,2},2}, c2={0,{2,3},2}, result;
        bool ok=cuts_merge(&c1,&c2,4,&result);
        assert(ok); assert(result.num_inputs==3);
        P();
    }

    T("Cut truth table");{
        FpgaBoolNetwork bn; bool_network_init(&bn);
        int a=bool_network_add_node(&bn,GATE_BUF); bool_network_set_pi(&bn,a);
        int b=bool_network_add_node(&bn,GATE_BUF); bool_network_set_pi(&bn,b);
        int x=bool_network_add_node(&bn,GATE_AND);
        bool_network_add_edge(&bn,a,x,0);
        bool_network_add_edge(&bn,b,x,1);
        FpgaCut cut; cut.root=x; cut.num_inputs=2;
        cut.input_set[0]=a; cut.input_set[1]=b;
        uint64_t tt=cut_truth_table(&bn,&cut);
        assert(tt==0x8); /* AND: only bit 3=1 */
        P();
    }

    T("FlowMap label");{
        FpgaBoolNetwork bn; bool_network_init(&bn);
        for(int i=0;i<8;i++){int n=bool_network_add_node(&bn,GATE_BUF);bool_network_set_pi(&bn,n);}
        int g[4]; for(int i=0;i<4;i++){g[i]=bool_network_add_node(&bn,GATE_AND);
            bool_network_add_edge(&bn,i*2,g[i],0);bool_network_add_edge(&bn,i*2+1,g[i],1);}
        int labels[16];
        assert(flowmap_label(&bn,4,labels)==0);
        assert(labels[g[0]]>=0);  /* labeled successfully */
        P();
    }

    T("FlowMap mapping");{
        FpgaBoolNetwork bn; bool_network_init(&bn);
        int a=bool_network_add_node(&bn,GATE_BUF);bool_network_set_pi(&bn,a);
        int b=bool_network_add_node(&bn,GATE_BUF);bool_network_set_pi(&bn,b);
        int c=bool_network_add_node(&bn,GATE_BUF);bool_network_set_pi(&bn,c);
        int x=bool_network_add_node(&bn,GATE_AND);
        bool_network_add_edge(&bn,a,x,0);bool_network_add_edge(&bn,b,x,1);
        int y=bool_network_add_node(&bn,GATE_OR);
        bool_network_add_edge(&bn,x,y,0);bool_network_add_edge(&bn,c,y,1);
        bool_network_set_po(&bn,y);
        bool_network_levelize(&bn);
        FpgaLutMapping result;
        int n=flowmap_mapping(&bn,4,&result);
        assert(n>0); assert(result.total_luts>0);
        lut_mapping_destroy(&result);
        P();
    }

    T("DAGMap area mapping");{
        FpgaBoolNetwork bn; bool_network_init(&bn);
        int a=bool_network_add_node(&bn,GATE_BUF);bool_network_set_pi(&bn,a);
        int b=bool_network_add_node(&bn,GATE_BUF);bool_network_set_pi(&bn,b);
        int c=bool_network_add_node(&bn,GATE_BUF);bool_network_set_pi(&bn,c);
        int x=bool_network_add_node(&bn,GATE_AND);
        bool_network_add_edge(&bn,a,x,0);bool_network_add_edge(&bn,b,x,1);
        int y=bool_network_add_node(&bn,GATE_AND);
        bool_network_add_edge(&bn,x,y,0);bool_network_add_edge(&bn,c,y,1);
        bool_network_levelize(&bn);
        FpgaLutMapping result;
        int n=dagmap_area_mapping(&bn,4,&result);
        assert(n>0);
        lut_mapping_destroy(&result);
        P();
    }

    T("Carry chain extraction");{
        FpgaBoolNetwork bn; bool_network_init(&bn);
        int a=bool_network_add_node(&bn,GATE_BUF);bool_network_set_pi(&bn,a);
        int b=bool_network_add_node(&bn,GATE_BUF);bool_network_set_pi(&bn,b);
        int c=bool_network_add_node(&bn,GATE_BUF);bool_network_set_pi(&bn,c);
        int x=bool_network_add_node(&bn,GATE_XOR);  /* sum = a^b */
        bool_network_add_edge(&bn,a,x,0);bool_network_add_edge(&bn,b,x,1);
        int ab_and=bool_network_add_node(&bn,GATE_AND);
        bool_network_add_edge(&bn,a,ab_and,0);bool_network_add_edge(&bn,b,ab_and,1);
        int ac_and=bool_network_add_node(&bn,GATE_AND);
        bool_network_add_edge(&bn,a,ac_and,0);bool_network_add_edge(&bn,c,ac_and,1);
        int o=bool_network_add_node(&bn,GATE_OR);  /* cout = ab|ac */
        bool_network_add_edge(&bn,ab_and,o,0);bool_network_add_edge(&bn,ac_and,o,1);
        FpgaLutMapping result;
        int n=techmap_extract_carry(&bn,&result);
        assert(n>=0);  /* may find 0 or more carry chains */
        lut_mapping_destroy(&result);
        P();
    }

    T("LUT mapping export to atoms");{
        FpgaBoolNetwork bn; bool_network_init(&bn);
        int a=bool_network_add_node(&bn,GATE_BUF);bool_network_set_pi(&bn,a);
        int b=bool_network_add_node(&bn,GATE_BUF);bool_network_set_pi(&bn,b);
        int x=bool_network_add_node(&bn,GATE_AND);
        bool_network_add_edge(&bn,a,x,0);bool_network_add_edge(&bn,b,x,1);
        bool_network_levelize(&bn);
        FpgaLutMapping result;
        flowmap_mapping(&bn,4,&result);
        FpgaAtomNetlist nl; atom_netlist_init(&nl);
        int cnt=lut_mapping_export_to_atoms(&result,&nl);
        assert(cnt==result.num_entries);
        lut_mapping_destroy(&result);
        P();
    }

    printf("\n=== Results: %d passed, %d failed ===\n",p,f);
    return f>0?1:0;
}
