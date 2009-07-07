/*******************************************************************************
*
* Computation of the Wilson loops for the static potential
*
*******************************************************************************/

#define MAIN_PROGRAM

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "io.h"
#include "random.h"
#include "error.h"
#include "geometry.h"
#include "memory.h"
#include "statistics.h"
#include "update.h"
#include "global.h"
#include "observables.h"
#include "suN.h"
#include "suN_types.h"
#include "dirac.h"
#include "linear_algebra.h"
#include "inverters.h"
#include "representation.h"
#include "utils.h"
#include "logger.h"

#include "cinfo.c"


/* HYP smearing parameters */
typedef struct _input_HYP {
  int nsteps;
  double weight[3];
  char type[256]; /* alldirections ; spatialonly */

  /* for the reading function */
  input_record_t read[6];

} input_HYP;

#define init_input_HYP(varname) \
{ \
  .read={\
    {"HYP smearing weight[0]", "HYP:weight0 = %lf", DOUBLE_T, &((varname).weight[0])},\
    {"HYP smearing weight[1]", "HYP:weight1 = %lf", DOUBLE_T, &((varname).weight[1])},\
    {"HYP smearing weight[2]", "HYP:weight2 = %lf", DOUBLE_T, &((varname).weight[2])},\
    {"HYP smearing number of steps", "HYP:nsteps = %d", INT_T, &((varname).nsteps)},\
    {"HYP smearing type", "HYP:type = %s", STRING_T, (varname).type},\
    {NULL, NULL, 0, NULL}\
  }\
}

typedef struct _input_wilson {
  int Tmax;
  int steps_1_0_0;
  int steps_1_1_0;
  int steps_1_1_1;
  int steps_2_1_0;
  int steps_2_1_1;
  int steps_2_2_1;

  /* for the reading function */
  input_record_t read[8];

} input_wilson;

#define init_input_wilson(varname) \
{ \
  .read={\
    {"Max temporal separation", "SP:Tmax = %d", INT_T, &((varname).Tmax)},\
    {"Number of steps for (1,0,0) loops", "SP:steps_1_0_0 = %d", INT_T, &((varname).steps_1_0_0)},\
    {"Number of steps for (1,1,0) loops", "SP:steps_1_1_0 = %d", INT_T, &((varname).steps_1_1_0)},\
    {"Number of steps for (1,1,1) loops", "SP:steps_1_1_1 = %d", INT_T, &((varname).steps_1_1_1)},\
    {"Number of steps for (2,1,0) loops", "SP:steps_2_1_0 = %d", INT_T, &((varname).steps_2_1_0)},\
    {"Number of steps for (2,1,1) loops", "SP:steps_2_1_1 = %d", INT_T, &((varname).steps_2_1_1)},\
    {"Number of steps for (2,2,1) loops", "SP:steps_2_2_1 = %d", INT_T, &((varname).steps_2_2_1)},\
    {NULL, NULL, 0, NULL}\
  }\
}


input_HYP HYP_var = init_input_HYP(HYP_var);
input_wilson wilson_var = init_input_wilson(wilson_var);

char cnfg_filename[256]="";
char list_filename[256]="";
char input_filename[256] = "input_file";
char output_filename[256] = "wilson.out";
enum { UNKNOWN_CNFG, DYNAMICAL_CNFG, QUENCHED_CNFG };


typedef struct {
  char string[256];
  int t, x, y, z;
  int nc, nf;
  double b, m;
  int n;
  int type;
} filename_t;


int parse_cnfg_filename(char* filename, filename_t* fn) {
  int hm;
  char *tmp = NULL;
  char *basename;

  basename = filename;
  while ((tmp = strchr(basename, '/')) != NULL) {
    basename = tmp+1;
  }            

#ifdef REPR_FUNDAMENTAL
#define repr_name "FUN"
#elif defined REPR_SYMMETRIC
#define repr_name "SYM"
#elif defined REPR_ANTISYMMETRIC
#define repr_name "ASY"
#elif defined REPR_ADJOINT
#define repr_name "ADJ"
#endif
  hm=sscanf(basename,"%*[^_]_%dx%dx%dx%d%*[Nn]c%dr" repr_name "%*[Nn]f%db%lfm%lfn%d",
      &(fn->t),&(fn->x),&(fn->y),&(fn->z),&(fn->nc),&(fn->nf),&(fn->b),&(fn->m),&(fn->n));
  if(hm==9) {
    fn->m=-fn->m; /* invert sign of mass */
    fn->type=DYNAMICAL_CNFG;
    return DYNAMICAL_CNFG;
  }
#undef repr_name

  double kappa;
  hm=sscanf(basename,"%dx%dx%dx%d%*[Nn]c%d%*[Nn]f%db%lfk%lfn%d",
      &(fn->t),&(fn->x),&(fn->y),&(fn->z),&(fn->nc),&(fn->nf),&(fn->b),&kappa,&(fn->n));
  if(hm==9) {
    fn->m = .5/kappa-4.;
    fn->type=DYNAMICAL_CNFG;
    return DYNAMICAL_CNFG;
  }

  hm=sscanf(basename,"%dx%dx%dx%d%*[Nn]c%db%lfn%d",
      &(fn->t),&(fn->x),&(fn->y),&(fn->z),&(fn->nc),&(fn->b),&(fn->n));
  if(hm==7) {
    fn->type=QUENCHED_CNFG;
    return QUENCHED_CNFG;
  }

  hm=sscanf(basename,"%*[^_]_%dx%dx%dx%d%*[Nn]c%db%lfn%d",
      &(fn->t),&(fn->x),&(fn->y),&(fn->z),&(fn->nc),&(fn->b),&(fn->n));
  if(hm==7) {
    fn->type=QUENCHED_CNFG;
    return QUENCHED_CNFG;
  }

  fn->type=UNKNOWN_CNFG;
  return UNKNOWN_CNFG;
}


void read_cmdline(int argc, char* argv[]) {
  int i, ai=0, ao=0, ac=0, al=0, am=0;
  FILE *list=NULL;

  for (i=1;i<argc;i++) {
    if (strcmp(argv[i],"-i")==0) ai=i+1;
    else if (strcmp(argv[i],"-o")==0) ao=i+1;
    else if (strcmp(argv[i],"-c")==0) ac=i+1;
    else if (strcmp(argv[i],"-l")==0) al=i+1;
    else if (strcmp(argv[i],"-m")==0) am=i;
  }

  if (am != 0) {
    print_compiling_info();
    exit(0);
  }

  if (ao!=0) strcpy(output_filename,argv[ao]);
  if (ai!=0) strcpy(input_filename,argv[ai]);

  error((ac==0 && al==0) || (ac!=0 && al!=0),1,"parse_cmdline [mk_wilsonloops.c]",
      "Syntax: mk_wilsonloops { -c <config file> | -l <list file> } [-i <input file>] [-o <output file>] [-m]");

  if(ac != 0) {
    strcpy(cnfg_filename,argv[ac]);
    strcpy(list_filename,"");
  } else if(al != 0) {
    strcpy(list_filename,argv[al]);
    error((list=fopen(list_filename,"r"))==NULL,1,"parse_cmdline [mk_wilsonloops.c]" ,
	"Failed to open list file\n");
    error(fscanf(list,"%s",cnfg_filename)==0,1,"parse_cmdline [mk_wilsonloops.c]" ,
	"Empty list file\n");
    fclose(list);
  }


}


int main(int argc,char *argv[]) {
  int i,t,p;
  char tmp[256];
  FILE* list;
  filename_t fpars;
  int c[3];
  suNg_field* smeared_g;

  /* setup process id and communications */
  read_cmdline(argc, argv);
  setup_process(&argc,&argv);

  /* logger setup */
  /* disable logger for MPI processes != 0 */
  logger_setlevel(0,70);
  if (PID!=0) { logger_disable(); }
  if (PID==0) { 
    sprintf(tmp,">%s",output_filename); logger_stdout(tmp);
    sprintf(tmp,"err_%d",PID); freopen(tmp,"w",stderr);
  }

  lprintf("MAIN",0,"Compiled with macros: %s\n",MACROS); 
  lprintf("MAIN",0,"PId =  %d [world_size: %d]\n\n",PID,WORLD_SIZE); 
  lprintf("MAIN",0,"input file [%s]\n",input_filename); 
  lprintf("MAIN",0,"output file [%s]\n",output_filename); 
  if (list_filename!=NULL) lprintf("MAIN",0,"list file [%s]\n",list_filename); 
  else lprintf("MAIN",0,"cnfg file [%s]\n",cnfg_filename); 


  /* read & broadcast parameters */
  parse_cnfg_filename(cnfg_filename,&fpars);

  HYP_var.weight[0]=HYP_var.weight[1]=HYP_var.weight[2]=0.;
  wilson_var.Tmax=GLB_T;
  wilson_var.steps_1_0_0=0;
  wilson_var.steps_1_1_0=0;
  wilson_var.steps_1_1_1=0;
  wilson_var.steps_2_1_0=0;
  wilson_var.steps_2_1_1=0;
  wilson_var.steps_2_2_1=0;
  read_input(glb_var.read,input_filename);
  read_input(HYP_var.read,input_filename);
  read_input(wilson_var.read,input_filename);
  GLB_T=fpars.t; GLB_X=fpars.x; GLB_Y=fpars.y; GLB_Z=fpars.z;
  
  error(fpars.type==UNKNOWN_CNFG,1,"mk_wilsonloops.c","Bad name for a configuration file");
  error(fpars.nc!=NG,1,"mk_wilsonloops.c","Bad NG");
  error(HYP_var.nsteps<1,1,"mk_wilsonloops.c","Bad HYP:nsteps value");
  error(strcmp(HYP_var.type,"alldirections")!=0 && strcmp(HYP_var.type,"spatialonly")!=0,1,"mk_wilsonloops.c","Bad HYP:type value");


  /* setup communication geometry */
  if (geometry_init() == 1) {
    finalize_process();
    return 0;
  }

  lprintf("MAIN",0,"Gauge group: SU(%d)\n",NG);
  lprintf("MAIN",0,"Fermion representation: " REPR_NAME " [dim=%d]\n",NF);
  lprintf("MAIN",0,"global size is %dx%dx%dx%d\n",GLB_T,GLB_X,GLB_Y,GLB_Z);
  lprintf("MAIN",0,"proc grid is %dx%dx%dx%d\n",NP_T,NP_X,NP_Y,NP_Z);
  lprintf("MAIN",0,"Fermion boundary conditions: %.2f,%.2f,%.2f,%.2f\n",bc[0],bc[1],bc[2],bc[3]);

  /* setup lattice geometry */
  geometry_mpi_eo();
  /* test_geometry_mpi_eo(); */

  lprintf("MAIN",0,"local size is %dx%dx%dx%d\n",T,X,Y,Z);
  lprintf("MAIN",0,"extended local size is %dx%dx%dx%d\n",T_EXT,X_EXT,Y_EXT,Z_EXT);

  lprintf("MAIN",0,"RLXD [%d,%d]\n",glb_var.rlxd_level,glb_var.rlxd_seed);
  rlxd_init(glb_var.rlxd_level,glb_var.rlxd_seed+PID);

  lprintf("MAIN",0,"HYP smearing weights: %f %f %f\n",HYP_var.weight[0],HYP_var.weight[1],HYP_var.weight[2]);
  lprintf("MAIN",0,"HYP smearing number of steps: %d\n",HYP_var.nsteps);
  lprintf("MAIN",0,"HYP smearing type: %s\n",HYP_var.type);

  lprintf("MAIN",0,"Maximum temporal extension of wilson loops: %d\n",wilson_var.Tmax);
  lprintf("MAIN",0,"Number of steps for (1,0,0) wilson loops: %d\n",wilson_var.steps_1_0_0);
  lprintf("MAIN",0,"Number of steps for (1,1,0) wilson loops: %d\n",wilson_var.steps_1_1_0);
  lprintf("MAIN",0,"Number of steps for (1,1,1) wilson loops: %d\n",wilson_var.steps_1_1_1);
  lprintf("MAIN",0,"Number of steps for (2,1,0) wilson loops: %d\n",wilson_var.steps_2_1_0);
  lprintf("MAIN",0,"Number of steps for (2,1,1) wilson loops: %d\n",wilson_var.steps_2_1_1);
  lprintf("MAIN",0,"Number of steps for (2,2,1) wilson loops: %d\n",wilson_var.steps_2_2_1);
    

  /* alloc global gauge fields */
  u_gauge=alloc_gfield(&glattice);
  smeared_g=alloc_gfield(&glattice);

  list=NULL;
  if(strcmp(list_filename,"")!=0) {
    error((list=fopen(list_filename,"r"))==NULL,1,"main [mk_mesons.c]" ,
	"Failed to open list file\n");
  }

  i=0;
  while(1) {

    if(list!=NULL)
      if(fscanf(list,"%s",cnfg_filename)==0 || feof(list)) break;

    i++;

    lprintf("MAIN",0,"Configuration from %s\n", cnfg_filename);
    /* NESSUN CHECK SULLA CONSISTENZA CON I PARAMETRI DEFINITI !!! */
    read_gauge_field(cnfg_filename);

    lprintf("TEST",0,"<p> %1.6f\n",avr_plaquette());

    full_plaquette();

    if(strcmp(HYP_var.type,"alldirections")==0) {
      for(p=0;p<HYP_var.nsteps/2;p++) {
        HYP_smearing(smeared_g,u_gauge,HYP_var.weight);
        HYP_smearing(u_gauge,smeared_g,HYP_var.weight);
      }
      if(HYP_var.nsteps%2==1)
        HYP_smearing(smeared_g,u_gauge,HYP_var.weight);
      else
        suNg_field_copy(smeared_g,u_gauge);
    } else {
      for(p=0;p<HYP_var.nsteps/2;p++) {
        spatialHYP_smearing(smeared_g,u_gauge,HYP_var.weight);
        spatialHYP_smearing(u_gauge,smeared_g,HYP_var.weight);
      }
      if(HYP_var.nsteps%2==1)
        spatialHYP_smearing(smeared_g,u_gauge,HYP_var.weight);
      else
        suNg_field_copy(smeared_g,u_gauge);
    }

/*    for(t=1;t<GLB_T;t++)*/
/*      wilsonloops(0,t,smeared_g);*/
    
    if(wilson_var.steps_1_0_0 != 0) {
      c[0]=1;c[1]=c[2]=0;
      for(t=1;t<wilson_var.Tmax;t++)
        ara_temporalwilsonloops(t,c,wilson_var.steps_1_0_0,smeared_g);
    }

    if(wilson_var.steps_1_1_0 != 0) {
      c[0]=1;c[1]=1;c[2]=0;
      for(t=1;t<wilson_var.Tmax;t++)
        ara_temporalwilsonloops(t,c,wilson_var.steps_1_1_0,smeared_g);
    }

    if(wilson_var.steps_1_1_1 != 0) {
      c[0]=1;c[1]=1;c[2]=1;
      for(t=1;t<wilson_var.Tmax;t++)
        ara_temporalwilsonloops(t,c,wilson_var.steps_1_1_1,smeared_g);
    }

    if(wilson_var.steps_2_1_0 != 0) {
      c[0]=2;c[1]=1;c[2]=0;
      for(t=1;t<GLB_T;t++)
        ara_temporalwilsonloops(t,c,wilson_var.steps_2_1_0,smeared_g);
    }

    if(wilson_var.steps_2_1_1 != 0) {
      c[0]=2;c[1]=1;c[2]=1;
      for(t=1;t<wilson_var.Tmax;t++)
        ara_temporalwilsonloops(t,c,wilson_var.steps_2_1_1,smeared_g);
    }
 
    if(wilson_var.steps_2_2_1 != 0) {
      c[0]=2;c[1]=2;c[2]=1;
      for(t=1;t<wilson_var.Tmax;t++)
        ara_temporalwilsonloops(t,c,wilson_var.steps_2_2_1,smeared_g);
    }
   
    if(list==NULL) break;
  }

  if(list!=NULL) fclose(list);

  finalize_process();
 
  free_gfield(u_gauge);
  free_gfield(smeared_g);
  
  return 0;
}

