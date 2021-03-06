/*
 * CitcomCU is a Finite Element Code that solves for thermochemical
 * convection within a three dimensional domain appropriate for convection
 * within the Earth's mantle. Cartesian and regional-spherical geometries
 * are implemented. See the file README contained with this distribution
 * for further details.
 * 
 * Copyright (C) 1994-2005 California Institute of Technology
 * Copyright (C) 2000-2005 The University of Colorado
 *
 * Authors: Louis Moresi, Shijie Zhong, and Michael Gurnis
 *
 * For questions or comments regarding this software, you may contact
 *
 *     Luis Armendariz <luis@geodynamics.org>
 *     http://geodynamics.org
 *     Computational Infrastructure for Geodynamics (CIG)
 *     California Institute of Technology
 *     2750 East Washington Blvd, Suite 210
 *     Pasadena, CA 91007
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 2 of the License, or any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/* This file contains the definitions of variables which are passed as arguments */
/* to functions across the whole filespace of CITCOM. #include this file everywhere !*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef USE_GGRD
#include "hc_ggrd.h"
#include "hc.h"
#endif
#include "zlib.h"




#define safe_malloc(n) safe_malloc_winfo((n),__FILE__,__LINE__)

#ifdef CITCOM_XMC_LOW_PREC
#define CITCOM_XMC_PREC float
#else
#define CITCOM_XMC_PREC double
#endif

#define CITCOM_EPS 1e-7	/* for == 0 test */

/* #define Malloc0 malloc */

#define LIDN 0x1
#define VBX 0x2
#define VBZ 0x4
#define VBY 0x8
#define TBX 0x10
#define TBZ 0x20
#define TBY 0x40
#define TZEDGE 0x80
#define TXEDGE 0x100
#define TYEDGE 0x200
#define VXEDGE 0x400
#define VZEDGE 0x800
#define VYEDGE 0x1000
#define INTX 0x2000
#define INTZ 0x4000
#define INTY 0x8000
#define SBX 0x10000
#define SBZ 0x20000
#define SBY 0x40000
#define FBX 0x80000
#define FBZ 0x100000
#define FBY 0x200000

#define OFFSIDE 0x400000
#define SIDEE 0x800000

#define LIDE 1

#define CITCOM_TRACER_EPS_MARGIN 1.0e-6
#define R_EARTH_KM 6371.0

#ifndef COMPRESS_BINARY
#define COMPRESS_BINARY "/usr/bin/compress"
#endif

#define MAX_LEVELS 12
#define MAX_F    10
#define MAX_S    30

#define MAX_NEIGHBORS 27

#define GGRD_MAX_NR_SLICE 5

//#define CU_MPI_MSG_LIM 100	/* this increase wasn't necessary */
#define CU_MPI_MSG_LIM 1000

/* Macros */

#define max(A,B) (((A) > (B)) ? (A) : (B))
#define min(A,B) (((A) < (B)) ? (A) : (B))
#define SWAP(a,b) {temp=(a);(a)=(b);(b)=temp;}

typedef float higher_precision;	/* matrix coeffs etc */
typedef double higher_precision1;	/* intermediate calculations for finding above coeffs */


/* Common structures */

struct Rect
{
	int numb;
	char overlay[40];
	float x1[40];
	float x2[40];
	float z1[40];
	float z2[40];
	float y1[40];
	float y2[40];
	float halfw[40];
	float mag[40];
};


struct Circ
{
	int numb;
	char overlay[40];
	float x[40];
	float z[40];
	float y[40];
	float rad[40];
	float mag[40];
	float halfw[40];
};


struct Harm
{
	int numb;
	int harms;
	char overlay[40];
	float off[40];
	float x1[40];
	float x2[40];
	float z1[40];
	float z2[40];
	float y1[40];
	float y2[40];
	float kx[20][40];
	float kz[20][40];
	float ky[20][40];
	float ka[20][40];
	float phx[20][40];
	float phz[20][40];
	float phy[20][40];
};

struct Erfc
{
	int numb;


};

struct RectBc
{
	int numb;
	char norm[40];
	float intercept[40];
	float x1[40];
	float x2[40];
	float z1[40];
	float z2[40];
	float halfw[40];
	float mag[40];
};


struct CircBc
{
	int numb;
	char norm[40];
	float intercept[40];
	float x[40];
	float z[40];
	float rad[40];
	float mag[40];
	float halfw[40];
};


struct PolyBc
{
	int numb;
	int order;
	char norm[40];
	float intercept[40];
	float x1[40];
	float x2[40];
	float z1[40];
	float z2[40];
	float ax[20][40];
	float az[20][40];
};


struct HarmBc
{
	int numb;
	int harms;
	char norm[40];
	float off[40];
	float intercept[40];
	float x1[40];
	float x2[40];
	float z1[40];
	float z2[40];
	float kx[20][40];
	float kz[20][40];
	float ka[20][40];
	float phx[20][40];
	float phz[20][40];
};


struct Shape_function_dA
{
	float vpt[8+1];
	float ppt[1+1];
};

struct Shape_function1_dA
{
	double vpt[6 * 4];
	double ppt[6 * 1];
};

struct Shape_function1
{
	double vpt[4 * 4];			/* node & gauss pt */
	double ppt[4 * 1];
};

struct Shape_function
{
	double vpt[8 * 8];			/* node & gauss pt */
	double spt[8 * 4];			/* node & gauss pt */
	double ppt[8 * 1];
};

struct Shape_function_dx
{
	float vpt[3 * 8 * 8];		/* dirn & node & gauss pt */
	float ppt[3 * 8 * 1];
};

struct Shape_function1_dx
{
	double vpt[2 * 4 * 4];		/* dirn & node & gauss pt */
	double ppt[2 * 4 * 1];
};

struct CC
{
	double vpt[3 * 3 * 8 * 8];
	double ppt[3 * 3 * 8 * 1];
};								/* dirn & node & gauss pt */

struct CCX
{
	double vpt[3 * 3 * 2 * 8 * 8];
	double ppt[3 * 3 * 2 * 8 * 1];
};								/* dirn & node & gauss pt */


struct EG
{
	higher_precision g[24][1];
};

struct EK2
{
	double k[8 * 8];
};

struct EK
{
	double k[24 * 24];
};

struct MEK
{
	double nint[9];
};

struct NK
{
	higher_precision *k;
	int *map;
};

struct COORD
{
	float centre[4];
	float size[4];
	float recip_size[4];
	float area;
};

struct SUBEL
{
	int sub[9];
};

struct ID
{
	int doff[6];
};								/* can  be 1 or 2 or 3 */
struct IEN
{
	int node[9];
};
struct FNODE
{
	float node[9];
};
struct SIEN
{
	int node[5];
};
struct LM
{
	struct
	{
		int doff[4];
	} node[9];
};

struct NEI
{
	int *nels;
	int *lnode;
	int *element;
};

struct Slabs
{
	float max_depth;
	float min_depth;
	float *dip[MAX_F];
	float *strike[MAX_F];
	float *width[MAX_F];
	float *depth[MAX_F];
	int total_node;
	int *vert;
	float age_max;
	float *age;
	int *node;
};

struct Crust
{
	float width;
	float thickness;
	float *T;
	int total_node;
	int *node;
};

struct Segment
{

	int zlayers;
	int nzlayer[40];
	float zzlayer[40];
	int xlayers;
	int nxlayer[40];
	float xxlayer[40];
	int ylayers;
	int nylayer[40];
	float yylayer[40];

	float plume_coord[4];
	float plume_radius;
	float plume_DT;
	float *Tz;
	float *Vx;
	int sym_nx;
};

struct BOUND
{
	int bound[4][3];
};

struct PASS
{
	int pass[4][3];
};

struct SPHERE
{
	float ri;
	float ro;
	float ti;
	float to;
	float fi;
	float fo;
	float corner[3][4];
	int nox;
	int noy;
	int noz;
	int nsf;
	int nno;
	int elx;
	int ely;
	int elz;
	int snel;
	int llmax;
	int *hindex[100];
	int hindice;
	float *harm_tpgt[2];
	float *harm_tpgb[2];
	float *harm_slab[2];
	float *harm_velp[2];
	float *harm_velt[2];
	float *harm_divg[2];
	float *harm_vort[2];
	float *harm_visc[2];
	float *harm_geoid[2];
	double *sx[4];
	double *con;
	double *det[5];
	double *tableplm[181];
	double *tablecosf[361];
	double *tablesinf[361];
	double tablesint[181];

	double *tableplm_n[182];
	double *tablecosf_n[362];
	double *tablesinf_n[362];
	struct SIEN *sien;

	double dircos[4][4];
  float *vtk_base;
  int vtk_base_init;
  
	int output_llmax;

	int lnox;
	int lnoy;
	int lnoz;
	int lnsf;
	int lnno;
	int lelx;
	int lely;
	int lelz;
	int lsnel;
	int lexs;
	int leys;

};

struct Parallel
{
	char machinename[160];
	int me;
	int nproc;
	int nprocx;
	int nprocz;
	int nprocy;
	int nprocxy;
	int nproczy;
	int nprocxz;
	int automa;
	int idb;
	int me_loc[4];
	int num_b;

	int *IDD[MAX_LEVELS];
	int *ELE_ORDER[MAX_LEVELS];
	int *NODE_ORDER[MAX_LEVELS];
	struct BOUND *NODE[MAX_LEVELS];
	struct BOUND NUM_NNO[MAX_LEVELS];
	struct BOUND NUM_PASS[MAX_LEVELS];
	struct BOUND NUM_ELE[MAX_LEVELS];
	struct BOUND *IDPASS[MAX_LEVELS];

	int TNUM_PASS[MAX_LEVELS];
	int START_ELE[MAX_LEVELS];
	int START_NODE[MAX_LEVELS];
	struct PASS NUM_NEQ[MAX_LEVELS];
	struct PASS NUM_NODE[MAX_LEVELS];
	struct PASS PROCESSOR[MAX_LEVELS];
	struct PASS *EXCHANGE_ID[MAX_LEVELS];
	struct PASS *EXCHANGE_NODE[MAX_LEVELS];

	int me_sph;
	int no_neighbors;
	int neighbors[MAX_NEIGHBORS];
	int *neighbors_rev;
	int traces_receive_number[MAX_NEIGHBORS];
	int traces_transfer_number[MAX_NEIGHBORS];
	int *traces_transfer_index[MAX_NEIGHBORS];
};

struct MESH_DATA
{								/* general information concerning the fe mesh */
	int nsd;					/* Spatial extent 1,2,3d */
	int dof;					/* degrees of freedom per node */
	int levmax;
	int levmin;
	int levels;
	int mgunitx;
	int mgunitz;
	int mgunity;
	int NEQ[MAX_LEVELS];		/* All other values refer to the biggest mesh (& lid)  */
	int NNO[MAX_LEVELS];
	int NNOV[MAX_LEVELS];
	int NLNO[MAX_LEVELS];
	int NPNO[MAX_LEVELS];
	int NEL[MAX_LEVELS];
	int NOX[MAX_LEVELS];
	int NOZ[MAX_LEVELS];
	int NOY[MAX_LEVELS];
	int NMX[MAX_LEVELS];
	int NXS[MAX_LEVELS];
	int NZS[MAX_LEVELS];
	int NYS[MAX_LEVELS];
	int ELX[MAX_LEVELS];
	int ELZ[MAX_LEVELS];
	int ELY[MAX_LEVELS];
	int LNDS[MAX_LEVELS];
	int LELS[MAX_LEVELS];
	int neqd;
	int neq;
	int nno;
	int nnov;
	int nlno;
	int npno;
	int nel;
	int snel;
	int nex[4];					/* general form of ... */
	int elx;
	int elz;
	int ely;
	int nnx[4];					/* general form of ... */
	int rnox;
	int rnoz;
	int rnoy;
	int nox;
	int noz;
	int noy;
	int nxs;
	int nzs;
	int nys;
	int nmx;
	int nsf;					/* nodes for surface observables */
	int toptbc;
	int bottbc;
	int topvbc;
	int botvbc;
	int sidevbc;

	char topvbc_file[500];
	char botvbc_file[500];
	char sidevbc_file[500];
	char gridfile[4][500];


	int periodic_x;
	int periodic_y;
  int slab_influx_side_bc;
  float slab_influx_z1,slab_influx_z2,slab_influx_y2;

  int shear_in_x_bc;
  float shear_in_x_val;
  
  int periodic_pin_or_filter;	/*  */
	double volume;
	float layer[4];				/* dimensionless dimensions */
	float lidz;
	float bl1width[4], bl2width[4], bl1mag[4], bl2mag[4];
	float hwidth[4], magnitude[4], offset[4], width[4];	/* grid compression information */
	int fnodal_malloc_size;
	int dnodal_malloc_size;
	int feqn_malloc_size;
	int deqn_malloc_size;
	int bandwidth;
	int null_source;
	int null_sink;
	int matrix_size[MAX_LEVELS];
};

struct HAVE
{								/* horizontal averages */
	float *T;
	float *C;
	float *Tadi;
	float *Vi;
	float *Rho;
	float *f;
	float *F;
	float *vrms;
	float *V[4];
};

struct SLICE
{								/* horizontally sliced data, including topography */
	float *tpg;
	float *tpgb;
	float *grv;
	float *geo;
	float *geok;
	float *grvk;
	float *grvb;
	float *geob;
	float *geobk;
	float *grvbk;
	float *tpgk;
	float *tpgbk;
	float *shflux;
	float *bhflux;
	float *cen_mflux;
	float *vxsurf[3];			/* surface velocity vectors */
	float *vline;				/* for kernels, velocity at force term */
	float *vlinek;
	float *tpglong;
	float *tpgblong;
	float *grvlong;
	float *geolong;
	float *grvblong;
	float *geoblong;
	float Nub;
	float Nut;

	int minlong;
	int maxlong;
};

struct BAVE
{
	float T;
	float Vi;
	double V[4];
};


struct TOTAL
{
	float melt_prod;
};

struct MONITOR
{
	char node_output[500][6];	/* recording the format of the output data */
	char sobs_output[500][6];	/* recording the format of the output data */
	int node_output_cols;
	int sobs_output_cols;

  int solution_cycles,solution_cycles_out;
  int visc_iter_count;
  int max_sdep_visc_iter;
	float time_scale,time_scale_ma;
	float length_scale;
        float viscosity_scale;
  float velo_scale;
  float tau_scale;
  float T_interior_max;
	float geoscale;
	float tpgscale;
	float grvscale;


	float delta_v_last_soln;
	float elapsed_time;
	float elapsed_time_vsoln;
	float elapsed_time_vsoln1;
	float reference_stress;
	float incompressibility;
  float vdotv,pdotp;
	float nond_av_heat_fl;
	float nond_av_adv_hfl;
	float cpu_time_elapsed;
	float cpu_time_on_vp_it;
	float cpu_time_on_forces;
	float cpu_time_on_mg_maps;
	float tpgkmag;
	float grvkmag;

	float Nusselt;
	float Vmax;
	float Vsrms;
	float Vrms;
	float Vrms_surface;
	float Vrms_base;
	float F_surface;
	float F_base;
	float Frat_surface;
	float Frat_base;
	float T_interior;
	float Sigma_max;
	float Sigma_interior;
	float Vi_average;

};

struct Radioactive_Heat
{
	int num;
	double total;
	double concen_u;
	double percent[10];
	double heat_g[10];
	double decay_t[10];
	double concen[10];
};


struct CONTROL
{
	int PID;

	char output_written_external_command[500];	/* a unix command to run when output files have been created */

	int ORTHO, ORTHOZ;			/* indicates levels of mesh symmetry */
	char B_is_good[MAX_LEVELS];	/* general information controlling program flow */
	char Ahat_is_good[MAX_LEVELS];	/* general information controlling program flow */
	char old_P_file[100];
	char data_file[100];
	char data_file2[100];

	char which_data_files[1000];
	char which_horiz_averages[1000];
	char which_running_data[1000];
	char which_observable_data[1000];
  int add_init_t_gradient;

	char PROBLEM_TYPE[20];		/* one of ... */
	int KERNEL;
	int CONVECTION;
	int SLAB;
	char GEOMETRY[20];			/* one of ... */
	int CART2D;
	int CART2pt5D;
	int CART3D;
	int Rsphere;
	int AXI;
	char SOLVER_TYPE[20];		/* one of ... */
	int DIRECT;
	int restart;
  int force_initial_stokes_iteration;
	int restart_frame;
  int restart_timesteps;
	int CONJ_GRAD;
	int NMULTIGRID;
	int EMULTIGRID;
	int DIRECTII;
	char NODE_SPACING[20];		/* turns into ... */
	int GRID_TYPE;
	int COMPRESS;
	int AVS;
	int CONMAN;
	int stokes;

  int freeze_surface_at_step;

  int check_t_irange,check_c_irange;

	float Ra_670, clapeyron670, transT670, width670;
	float Ra_410, clapeyron410, transT410, width410;

  #define CINIT_SPHERE_PAR_N 6 /* x,y,z,r,cin,cout = number of sphere parameters */
  int cinit_sphere;
  float cinit_sphere_par[CINIT_SPHERE_PAR_N];
  
#ifdef USE_GGRD
  struct ggrd_master ggrd;

  int ggrd_t_slab_slice,ggrd_c_slab_slice;
  float ggrd_t_slab_theta_bound[GGRD_MAX_NR_SLICE],ggrd_c_slab_theta_bound[GGRD_MAX_NR_SLICE];

  struct ggrd_gt ggrd_ss_grd[4];
  int ggrd_mat_limit_prefactor,ggrd_mat_is_3d;
  char ggrd_mat_depth_file[1000],ggrd_flavor_gfile[1000],ggrd_flavor_dfile[1000];
  
#endif

	int adi_heating, visc_heating;
	int composition;
  int composition_neutralize_buoyancy;
  int composition_init_checkerboard;
	float z_comp;

	int dfact;
	double penalty;
	int augmented_Lagr;
	double augmented;
	int faults;
	int NASSEMBLE;
	int comparison;
	int crust;
	float plate_vel;
  float sub_vel;
	float plate_age;

  //float tole_comp;

	float sob_tolerance;

	float Atemp;
	float Acomp;
	float VBXtopval;
	float VBXbotval;
	float VBYtopval;
	float VBYbotval;

	float TBCtopval;
	float TBCbotval;

  float TBCbotval_side,TBCbotval_side_xapply;

	float Q0, Q0ER;

	int precondition;
	int vprecondition;
	int keep_going;
	int v_steps_low;
	int v_steps_high;
	int v_steps_upper;
	int max_vel_iterations;
	int p_iterations;
	int max_same_visc;
	float max_res_red_each_p_mg;
	float sub_stepping_factor;
	int mg_cycle;
	int true_vcycle;
	int down_heavy;
	int up_heavy;
	int depth_dominated;
	int eqn_viscosity;
	int eqn_zigzag;
	int verbose;
	double accuracy;
	double vaccuracy;

	int total_iteration_cycles;
	int total_v_solver_calls;

  int gzdir,vtk_pressure_out,vtk_vgm_out,vtk_viscosity_out,vtk_e2_out,vtk_stress2_out,vtk_stress_3D,
    vtk_ddpart_out,ele_pressure_out;
  
	int record_every;
	int record_all_until;

	int print_convergence;
	int sdepv_print_convergence;

	/* modules */
	int MELTING_MODULE;
	int CHEMISTRY_MODULE;

};

struct DATA
{
	float layer_km;
	float length_scale;
	float grav_acc;
	float therm_exp;
	float therm_exp_factor;
	float Cp;
	float therm_diff;
	float therm_diff_factor;
	float visc_factor;
	float therm_cond;
	float density;
	float res_density;
	float res_density_X;
	float melt_density;
	float density_above;
	float gas_const;
	float surf_heat_flux;
	float ref_viscosity;
	float melt_viscosity;
	float permeability;
	float grav_const;
	float surf_temp;
	float disptn_number;
	float youngs_mod;
	float Te;
	float ref_temperature;
	float Tsurf;
	float T_sol0;
	float T_adi0;
	float T_adi1;
	float buoy_flux;
	float delta_S;
	float dTsol_dz;
	float dTsol_dF;
	float dT_dz;
};

struct TRACES
{

	float VO[4];
	float Vpred[4];
  CITCOM_XMC_PREC XMC[4];
  CITCOM_XMC_PREC XMCpred[4];
	int C12;
	int leave;
	int CElement;
};


struct All_variables
{
#include "Convection_variables.h"
#include "viscosity_descriptions.h"
#include "temperature_descriptions.h"
#include "advection.h"

	FILE *fp;
	FILE *filed[20];

	struct SPHERE sphere;
	struct HAVE Have;
	struct BAVE Bulkave;
	struct TOTAL Total;
	struct MESH_DATA mesh;
	struct MESH_DATA lmesh;
	struct CONTROL control;
	struct MONITOR monitor;
	struct DATA data;
	struct SLICE slice;
	struct COORD *eco;
	struct IEN *ien;			/* global */
	struct SIEN *sien;
	struct TRACES *traces;
	struct ID *id;
	struct COORD *ECO[MAX_LEVELS];
	struct IEN *IEN[MAX_LEVELS];	/* global at each level */
	struct ID *ID[MAX_LEVELS];
	struct NEI NEI[MAX_LEVELS];
	struct SUBEL *EL[MAX_LEVELS];
	struct EG *elt_del[MAX_LEVELS];
	struct EK *elt_k[MAX_LEVELS];
	struct FNODE *TWW[MAX_LEVELS];
	struct Segment segment;
	struct Slabs slabs;
	struct Crust crust;
	struct Radioactive_Heat rad_heat;

	struct Parallel parallel;

	higher_precision *Eqn_k1[MAX_LEVELS];
	higher_precision *Eqn_k2[MAX_LEVELS];
	higher_precision *Eqn_k3[MAX_LEVELS];
	int *Node_map[MAX_LEVELS];
	int *Node_eqn[MAX_LEVELS];
	int *Node_k_id[MAX_LEVELS];
  
  int debug;
	float *RVV[MAX_NEIGHBORS], *PVV[MAX_NEIGHBORS];
	CITCOM_XMC_PREC  *RXX[MAX_NEIGHBORS], *PXX[MAX_NEIGHBORS];
	int *RINS[MAX_NEIGHBORS], *PINS[MAX_NEIGHBORS];

	float *VO[4];
	float *Vpred[4];
  CITCOM_XMC_PREC  *XMC[4];
  CITCOM_XMC_PREC *XMCpred[4];
	int *C12;		/* this should be unsigned short */
	int *traces_leave;
	int *traces_leave_index;
	int *CElement;

  int **tflavors;		/* this should also be unsigned short */
  int tracers_add_flavors;
  int tracers_track_strain;	/* track scalar strain */
  int tracers_track_fse;	/* track full FSE */
  CITCOM_XMC_PREC *tracer_strain;
  
  int *tmaxflavor;

  int tracers_assign_dense_only;
  float tracers_assign_dense_fraction,tracers_dense_frac;
  
	int *RG[4];
	double *XRG[4];
	double *XP[4], XG1[4], XG2[4];
	double *BI[MAX_LEVELS];		/* inv of  diagonal elements of K matrix */
	double *BPI[MAX_LEVELS];
	float *V[4];				/* velocity X[dirn][node] can save memory */
	float *V1[4];				/* velocity X[dirn][node] can save memory */
	float *Vest[4];				/* velocity X[dirn][node] can save memory */

	float *heating_adi, *heating_visc, *heating_latent;

	double *P, *F, *H, *S, *U;
	float *stress;
	float *Psi;
	float *NP;
	float *heatflux;
	float *heatflux_adv;
	float *edot;				/* strain rate invariant */
	float *Mass;				/* lumped mass matrix (diagonal elements) for p-g solver etc. */
	float *tw;
	float *X[4], *SX[4];
	float *XX[MAX_LEVELS][4], *SXX[MAX_LEVELS][4], *MASS[MAX_LEVELS];

	float *Fas670, *Fas410, *Fas670_b, *Fas410_b;


	float *Vi, *EVi;
	float *diffusivity, *expansivity;
	float *T, *C, *CE, *buoyancy;
  int **CF;			/* tracer flavors */
  float *strain;			/* strain */
  
	float *Tdot;
	float *VI[MAX_LEVELS];		/* viscosity has to soak down to all levels */
	float *EVI[MAX_LEVELS];		/* element viscosity has to soak down to all levels */
	float *VB[4], *TB[4];		/* boundary conditions for V,T defined everywhere */
	float *TW[MAX_LEVELS];		/* nodal weightings */
  float *Vddpart;			/* onyl max level */

#ifdef CITCOM_ALLOW_ANISOTROPIC_VISC
  float *VI2[MAX_LEVELS],*EVI2[MAX_LEVELS];
  float *VIn1[MAX_LEVELS],*EVIn1[MAX_LEVELS];
  float *VIn2[MAX_LEVELS],*EVIn2[MAX_LEVELS];
  float *VIn3[MAX_LEVELS],*EVIn3[MAX_LEVELS];
  unsigned char *avmode[MAX_LEVELS];
#endif
#ifdef USE_GGRD
  float *VIP;			/* viscosity prefactor */
#endif

	int *surf_node, *surf_element;
	int *mat;					/* properties of mat */
	unsigned int *node;			/* properties of node */
	unsigned int *NODE[MAX_LEVELS];
	unsigned int *Element;
	unsigned int *eqn;
	unsigned int *EQN[MAX_LEVELS];

	double *temp;
	double **global_K;			/* direct solver stuff */
	double **factor_K;
	double *global_F;
	struct LM *lmd;
	struct LM *lm;
	struct LM *LMD[MAX_LEVELS];

	struct Shape_function_dx *GNX[MAX_LEVELS];
	struct Shape_function_dA *GDA[MAX_LEVELS];
	struct Shape_function_dx *gNX;
	struct Shape_function_dA *gDA;


	struct Shape_function1 M;	/* master-element shape funtions */
	struct Shape_function1_dx Mx;
	struct Shape_function N;
	struct Shape_function_dx Nx;
	struct Shape_function1 L;	/* master-element shape funtions */
	struct Shape_function1_dx Lx;

	void (*build_forcing_term) ();
	void (*iterative_solver) ();
	void (*next_buoyancy_field) ();
	void (*obtain_gravity) ();
	void (*problem_settings) ();
	void (*problem_derived_values) ();
	void (*problem_allocate_vars) ();
	void (*problem_boundary_conds) ();
	void (*problem_node_positions) ();
	void (*problem_update_node_positions) ();
	void (*problem_initial_fields) ();
	void (*problem_update_bcs) ();
	void (*special_process_new_velocity) ();
	void (*special_process_new_buoyancy) ();
	void (*solve_stokes_problem) ();
	void (*solver_allocate_vars) ();
	void (*transform) ();

	float (*node_space_function[3]) ();

};

/* To regenerate function prototypes:
 * 	1. Comment out the #include line below, and save this file
 * 	2. Run command: cproto -q -p -f 2 *.c > prototypes.h
 * 	3. Uncomment the #include line below, and save this file

 could also use 
  cproto -DCITCOM_ALLOW_ANISOTROPIC_VISC -DUSE_GGRD -q -p -I. -f2 *.c > prototypes.h

  cproto -DCITCOM_ALLOW_ANISOTROPIC_VISC -DUSE_GGRD -q -p -I. -I$HOME/progs/src/hc-svn/ -I$GMTHOME/include/ -f2 *.c > prototypes.h


 */ 
void twiddle_thumbs(struct All_variables *, int ); /* somehow, cproto
						      does not pick up
						      these functions?! */
void convection_initial_temperature_and_comp_ggrd(struct All_variables *);
void set_2dc_defaults(struct All_variables *);
void remove_horiz_ave(struct All_variables *, float *, float *, int );
void output_velo_related_gzdir(struct All_variables *, int);
void output_velo_related(struct All_variables *, int);

#ifdef CITCOM_XMC_LOW_PREC 
#include "prototypes_low.h"
#else
#include "prototypes.h"
#endif
