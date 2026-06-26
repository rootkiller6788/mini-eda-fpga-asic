#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "dnn_isa.h"
#include "systolic_array.h"
#include "pe_microarch.h"
#include "buffer_hierarchy.h"
#include "accel_roofline.h"

static int passed=0,failed=0;
#define TEST(n) do{printf("  RUN  %s...",n);int _f=failed;
#define DONE() if(failed==_f){printf(" PASSED\n");passed++;}}while(0)

void t_isa_init(void){TEST("isa_init");dnn_program_t p;dnn_program_init(&p,"t");assert(p.instr_count==0&&p.version==1);DONE();}
void t_isa_emit(void){TEST("isa_emit");dnn_program_t p;dnn_program_init(&p,"e");int32_t r=dnn_alloc_register(&p,"r",DNN_DTYPE_FP32);assert(r>=0);dnn_emit_halt(&p);assert(p.instr_count==1);DONE();}
void t_isa_enc(void){TEST("isa_enc");dnn_program_t p;dnn_program_init(&p,"c");int32_t r=dnn_alloc_register(&p,"r",DNN_DTYPE_INT8);dnn_emit_scalar_op(&p,DNN_OP_ADD,DNN_DTYPE_INT8,r,r,r);uint8_t b[16];dnn_encode_instruction(&p.instructions[0],b,16);assert(b[0]==DNN_OP_ADD);dnn_instruction_t d;assert(dnn_decode_instruction(b,16,&d));assert(d.opcode==DNN_OP_ADD);DONE();}
void t_isa_val(void){TEST("isa_val");dnn_program_t p;dnn_program_init(&p,"v");assert(dnn_isa_validate(&p));DONE();}
void t_isa_stats(void){TEST("isa_stats");dnn_program_t p;dnn_program_init(&p,"s");int32_t r0=dnn_alloc_register(&p,"r0",DNN_DTYPE_FP32);int32_t r1=dnn_alloc_register(&p,"r1",DNN_DTYPE_FP32);dnn_emit_scalar_op(&p,DNN_OP_ADD,DNN_DTYPE_FP32,r0,r1,r0);dnn_emit_vector_op(&p,DNN_OP_VADD,DNN_DTYPE_FP16,r0,r1,r0,16);dnn_emit_matmul(&p,DNN_DTYPE_FP16,r0,r1,r0,32,32,64);dnn_isa_stats_t st;dnn_isa_collect_stats(&p,&st);assert(st.total_instr==3&&st.scalar_ops==1&&st.vector_ops==1&&st.matrix_ops==1);DONE();}
void t_isa_exec(void){TEST("isa_exec");dnn_program_t p;dnn_program_init(&p,"x");dnn_emit_halt(&p);dnn_exec_context_t c;dnn_exec_init(&c,&p);assert(c.running);dnn_exec_run(&c,100);assert(c.halted);DONE();}
void t_pe_mac(void){TEST("pe_mac");assert(fabs(pe_compute_mac(2,3,1)-7.0)<1e-6);DONE();}
void t_pe_act(void){TEST("pe_act");assert(fabs(pe_relu(5)-5)<1e-6&&fabs(pe_relu(-3))<1e-6);assert(fabs(pe_sigmoid(0)-0.5)<1e-6);DONE();}
void t_pe_pipe(void){TEST("pe_pipe");pe_state_t pe;pe_config_t c=pe_default_config();pe_init(&pe,0,0,0,&c);pe_load_weight(&pe,2);pe_load_input(&pe,3);int i;for(i=0;i<10;i++)pe_cycle(&pe);assert(fabs(pe_read_output(&pe)-6.0)<1e-6);DONE();}
void t_pe_prec(void){TEST("pe_prec");double v=5.7;pe_convert_precision(&v,PE_FP32,PE_INT8);assert(fabs(v-6.0)<1e-6);DONE();}
void t_sa_mm(void){TEST("sa_mm");systolic_array_t sa;sa_config_t c=sa_default_config();c.rows=4;c.cols=4;sa_init(&sa,&c);assert(sa.config.rows==4&&sa.config.cols==4);sa_load_weights(&sa,(double[]){1},1,1);sa_load_inputs(&sa,(double[]){2},1,1);sa_run_cycles(&sa,20);sa_stats_t s;sa_collect_stats(&sa,&s);assert(s.mac_operations>0||s.cycles>0);sa_free(&sa);DONE();}
void t_sa_df(void){TEST("sa_df");systolic_array_t sa;sa_config_t c=sa_default_config();c.rows=4;c.cols=4;c.dataflow=SA_DATAFLOW_WEIGHT_STATIONARY;sa_init(&sa,&c);sa_load_weights(&sa,(double[]){1,0,0,1},2,2);sa_load_inputs(&sa,(double[]){2,2,2,2},2,2);sa_run_to_completion(&sa);sa_stats_t s;sa_collect_stats(&sa,&s);assert(s.cycles>0);sa_free(&sa);DONE();}
void t_buf(void){TEST("buf");buffer_hierarchy_t bh;buf_hierarchy_init(&bh);buf_hierarchy_add_level(&bh,BUF_LEVEL_L2_GLOBAL,"L2",256*1024,10,200.0);assert(bh.level_count==1);buf_level_t*l2=buf_hierarchy_get_level(&bh,BUF_LEVEL_L2_GLOBAL);assert(l2&&l2->total_size_bytes==256*1024);uint32_t seg=buf_level_alloc(l2,512,"d");assert(seg!=(uint32_t)-1);buf_level_free(l2,seg);assert(l2->free_bytes==256*1024);DONE();}
void t_dma(void){TEST("dma");buffer_hierarchy_t bh;buf_hierarchy_init(&bh);dma_engine_init(&bh.dma,4,100.0);buf_hierarchy_add_level(&bh,BUF_LEVEL_L2_GLOBAL,"L2",4096,5,100.0);buf_hierarchy_add_level(&bh,BUF_LEVEL_DRAM,"DRAM",1<<20,100,50.0);dma_descriptor_t d;memset(&d,0,sizeof(d));d.direction=DMA_DRAM_TO_L2;d.src_level=buf_hierarchy_get_level(&bh,BUF_LEVEL_DRAM);d.dst_level=buf_hierarchy_get_level(&bh,BUF_LEVEL_L2_GLOBAL);d.size_bytes=256;d.pattern=BUF_PATTERN_SEQUENTIAL;int ch=dma_submit(&bh.dma,&d);assert(ch>=0);while(!dma_is_idle(&bh.dma))dma_tick(&bh.dma,&bh);assert(bh.dma.total_bytes_transferred==256);DONE();}
void t_roof(void){TEST("roof");roof_accel_spec_t s;memset(&s,0,sizeof(s));s.num_mac_units=4096;s.clock_ghz=1.0;s.num_cores=1;s.peak_dram_bw_gb_s=100.0;s.peak_sram_bw_gb_s=500.0;assert(fabs(roofline_compute_peak_gflops(&s)-8192.0)<10.0);assert(fabs(roofline_compute_ridge_point(&s)-81.92)<1.0);assert(fabs(roofline_amdahl_speedup(1.0,10.0)-10.0)<1e-6);DONE();}
void t_dse(void){TEST("dse");roof_dse_point_t pts[2];memset(pts,0,sizeof(pts));pts[0].array_rows=32;pts[0].array_cols=32;pts[0].sram_kb=256;pts[0].dram_bw_gb_s=100;pts[0].clock_ghz=1.0;pts[1].array_rows=64;pts[1].array_cols=64;pts[1].sram_kb=512;pts[1].dram_bw_gb_s=200;pts[1].clock_ghz=1.0;roofline_model_t rm;roof_accel_spec_t ss;memset(&ss,0,sizeof(ss));ss.num_mac_units=4096;ss.clock_ghz=1.0;ss.num_cores=1;ss.peak_dram_bw_gb_s=200.0;ss.peak_sram_bw_gb_s=500.0;roofline_init(&rm,&ss);roofline_dse_evaluate(&rm,pts,2,1e12,1e9);assert(pts[0].projected_tops>0);roof_dse_point_t best;roofline_dse_find_optimal(pts,2,&best,false);assert(best.projected_tops>0);DONE();}
void t_integ(void){TEST("integ");dnn_program_t p;dnn_program_init(&p,"ig");int32_t r0=dnn_alloc_register(&p,"i",DNN_DTYPE_FP32);int32_t r1=dnn_alloc_register(&p,"w",DNN_DTYPE_FP32);int32_t r2=dnn_alloc_register(&p,"o",DNN_DTYPE_FP32);dnn_emit_load(&p,DNN_DTYPE_FP32,r0,0,1024);dnn_emit_load(&p,DNN_DTYPE_FP32,r1,0x1000,1024);dnn_emit_matmul(&p,DNN_DTYPE_FP32,r2,r0,r1,16,16,16);dnn_emit_halt(&p);assert(p.instr_count==4);dnn_exec_context_t c;dnn_exec_init(&c,&p);dnn_exec_run(&c,10000);assert(c.halted);DONE();}

int main(void){
    printf("========================================\n");
    printf("  AI Accelerator Design Test Suite\n");
    printf("========================================\n\n");
    printf("\n[DNN ISA]\n");
    t_isa_init();
    t_isa_emit();
    t_isa_enc();
    t_isa_val();
    t_isa_stats();
    t_isa_exec();
    printf("\n[PE Microarch]\n");
    t_pe_mac();
    t_pe_act();
    t_pe_pipe();
    t_pe_prec();
    printf("\n[Systolic Array]\n");
    t_sa_mm();
    t_sa_df();
    printf("\n[Buffer Hierarchy]\n");
    t_buf();
    t_dma();
    printf("\n[Roofline Model]\n");
    t_roof();
    t_dse();
    printf("\n[Integration]\n");
    t_integ();
    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n",passed,failed);
    printf("========================================\n");
    return failed>0?1:0;
}
