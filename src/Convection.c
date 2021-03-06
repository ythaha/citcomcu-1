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

/* Assumes parameter list is opened and reads the things it needs. 
   Variables are initialized etc, default values are set */


#include <math.h>
#include <malloc.h>
#include <sys/types.h>
#include "element_definitions.h"
#include "global_defs.h"
#include <stdlib.h>				/* for "system" command */
#include <string.h>
void convection_initial_markers(struct All_variables *,int );


void set_convection_defaults(struct All_variables *E)
{

  /* 0: purely thermal 1: composition and temperature 2: purely compositional */
  input_boolean("composition", &(E->control.composition), "0", E->parallel.me);
  input_boolean("composition_init_checkerboard", 
		&(E->control.composition_init_checkerboard), "0", E->parallel.me);

  input_boolean("composition_neutralize_buoyancy",
		&(E->control.composition_neutralize_buoyancy), "0", E->parallel.me);
  if(E->control.composition_neutralize_buoyancy && E->parallel.me == 0)
    fprintf(stderr,"WARNING: reducing thermal buoyanyc to zero in composition=1 regions\n");

  /* sphere */
  input_boolean("cinit_sphere",&(E->control.cinit_sphere),"off", E->parallel.me);
  input_float_vector("cinit_sphere_par",CINIT_SPHERE_PAR_N,E->control.cinit_sphere_par, E->parallel.me);
  
	
  input_int("tracers_add_flavors", &(E->tracers_add_flavors), "0", E->parallel.me);

  /* should be zero or one */
  input_boolean("tracers_track_strain",&(E->tracers_track_strain), "0", E->parallel.me);
  input_boolean("tracers_track_fse",&(E->tracers_track_fse), "0", E->parallel.me); /* keep track of full tensor */
  if(E->tracers_track_fse && (!E->tracers_track_strain)) /*  */
    myerror("need basic strain tracking for FSE tracking",E);

  input_float("tracers_assign_dense_fraction",&(E->tracers_assign_dense_fraction),"1.0",E->parallel.me);

  /* how many tracers maximum upon advection, was 2 */
  input_float("marker_max_factor",&(E->advection.marker_max_factor),"2.0",E->parallel.me);
  
  if(E->tracers_assign_dense_fraction < 1.0){
    force_report(E,"WARNING: only assigning tracers to elements which are predominantly dense");
    if(E->parallel.me == 0)fprintf(stderr,"assuming a fraction of %g%%\n",E->tracers_assign_dense_fraction * 100.0);
    E->tracers_assign_dense_only = 1;
  }else{
    E->tracers_assign_dense_only = 0;
  }
  
  if(E->control.composition)
    E->next_buoyancy_field = PG_timestep_particle;
  else
    E->next_buoyancy_field = PG_timestep;
  
  E->special_process_new_velocity = PG_process;
  E->special_process_new_buoyancy = twiddle_thumbs;
  E->problem_settings = read_convection_settings;
  E->problem_derived_values = convection_derived_values;
  E->problem_allocate_vars = convection_allocate_memory;
  E->problem_boundary_conds = convection_boundary_conditions;
  E->problem_initial_fields = convection_initial_fields;
  E->problem_node_positions = node_locations;
  E->problem_update_node_positions = twiddle_thumbs;
  E->problem_update_bcs = twiddle_thumbs;
  
  sprintf(E->control.which_data_files, "Temp,Strf,Pres");
  sprintf(E->control.which_horiz_averages, "Temp,Visc,Vrms");
  sprintf(E->control.which_running_data, "Step,Time,");
  sprintf(E->control.which_observable_data, "Shfl");
  
  return;
}

void read_convection_settings(struct All_variables *E)
{
	char tmp_string[500], tmp1_string[500];

	/* parameters */
	int m;

	m = E->parallel.me;

	input_float("rayleigh", &(E->control.Atemp), "essential", m);
	if(E->control.composition)
	  input_float("rayleigh_comp", &(E->control.Acomp), "essential", m);

	input_boolean("halfspace", &(E->convection.half_space_cooling), "off", m);
	input_float("halfspage", &(E->convection.half_space_age), "nodefault", m);



	input_int("num_perturbations", &(E->convection.number_of_perturbations), "0,0,32", m);
	input_boolean("random_t_init",&E->convection.random_t_init,"off", m);
#ifndef USE_GGRD
	if(E->convection.random_t_init)
	  myerror("random T init as per random_t_init only works for GGRD version\n",E);
#endif	

	input_float_vector("perturbmag", E->convection.number_of_perturbations, E->convection.perturb_mag, m);
	input_float_vector("perturbk", E->convection.number_of_perturbations, E->convection.perturb_k, m);
	input_float_vector("perturbm", E->convection.number_of_perturbations, E->convection.perturb_mm, m);
	input_float_vector("perturbl", E->convection.number_of_perturbations, E->convection.perturb_ll, m);

	input_string("prevT", E->convection.old_T_file, "initialize", m);

	if(E->control.restart)
	{
		input_int("restart_timesteps", &(E->control.restart_timesteps), "0", m); /* to
											    save
											    the
											    initial
											    restart
											    step */
		/* set the timestep counter to the restart timestep */
		E->monitor.solution_cycles = E->control.restart_timesteps;

		input_string("oldfile", tmp1_string, "initialize", m);
		input_string("use_scratch", tmp_string, "local", m);
		if(strcmp(tmp_string, "local") == 0)
			strcpy(E->convection.old_T_file, tmp1_string);
		else
			sprintf(E->convection.old_T_file, "/scratch_%s/%s/%s", E->parallel.machinename, tmp_string, tmp1_string);

	}


	advection_diffusion_parameters(E);

	return;
}

/* =================================================================
   Any setup which relates only to the convection stuff goes in here
   ================================================================= */

void convection_derived_values(struct All_variables *E)
{
	return;
}

void convection_allocate_memory(struct All_variables *E)
{
	advection_diffusion_allocate_memory(E);
	return;
}

/* ============================================ */

void convection_initial_fields(struct All_variables *E)
{
  int i,j,lim,need_space;
  force_report(E,"ok16a");
	if(E->control.composition)
	{

	  if(E->tracers_assign_dense_only){
	    if(E->parallel.me == 0)
	      fprintf(stderr,"WARNING: assigning only to %g%% of elements, not a good idea!\n",
		      E->tracers_assign_dense_fraction*100);
	    E->advection.markers = (int)((float)E->advection.markers_per_ele * (float) E->mesh.nel *
					 E->tracers_assign_dense_fraction);

	  }else{
		E->advection.markers = E->advection.markers_per_ele * E->mesh.nel;
	  }
		E->advection.markers = E->advection.markers * E->lmesh.volume / E->mesh.volume;
		/* default was twice as many tracers allowed, now a parameter */
		E->advection.markers_uplimit = (int)((float)E->advection.markers * E->advection.marker_max_factor);
		if(E->parallel.me == 0)fprintf(stderr, "amarkers: %d lmesh.volume %g volume %g\n", E->advection.markers, E->lmesh.volume, E->mesh.volume);
		for(i = 1; i <= E->mesh.nsd; i++)
		{
			E->VO[i] = (float *)safe_malloc((E->advection.markers_uplimit + 1) * sizeof(float));
			if(!(E->VO[i]))myerror("Convection: mem error: E->V0",E);

			E->Vpred[i] = (float *)safe_malloc((E->advection.markers_uplimit + 1) * sizeof(float));
			if(!(E->Vpred[i]))myerror("Convection: mem error: E->Vpred",E);

			E->XMCpred[i] = (CITCOM_XMC_PREC *)safe_malloc((E->advection.markers_uplimit + 1) * sizeof(CITCOM_XMC_PREC));
			if(!(E->XMCpred[i]))myerror("Convection: mem error: E->XMCpred",E);

			E->XMC[i] = (CITCOM_XMC_PREC *)safe_malloc((E->advection.markers_uplimit + 1) * sizeof(CITCOM_XMC_PREC));
			if(!(E->XMC[i]))myerror("Convection: mem error: E->XMC",E);

			if(i==1){ /* those should only get allocated once */
			  E->C12 = (int *)safe_malloc((E->advection.markers_uplimit + 1) * sizeof(int));
			  if(!(E->C12))myerror("Convection: mem error: E->C12",E);
			  
			  E->traces_leave = (int *)safe_malloc((E->advection.markers_uplimit + 1) * sizeof(int));
			  if(!(E->traces_leave))myerror("Convection: mem error: E->traces_leave",E);

			  E->CElement = (int *)safe_malloc((E->advection.markers_uplimit + 1) * sizeof(int));
			  if(!(E->CElement))myerror("Convection: mem error: E->CElement",E);
			  if(E->tracers_track_strain){
			    /* strain */
			    report(E, "tracking total strain");
			    /* scalar strain or FSE ?*/
			    need_space = (E->tracers_track_fse)?(10):(1);
			    E->tracer_strain = (CITCOM_XMC_PREC *)calloc((E->advection.markers_uplimit + 1) * 
									 need_space,
									 sizeof(CITCOM_XMC_PREC));
			    if(!E->tracer_strain)
			      myerror("Convection: mem error: tracer_strain",E);
			    if(E->tracers_track_fse){
			    /* if FSE tracking, init as unity matrix */
			       lim = E->advection.markers_uplimit + 1;
			       for(j=0;j < lim;j++){
			         E->tracer_strain[j*need_space + 1] = E->tracer_strain[j*need_space + 5] = 
				   E->tracer_strain[j*need_space + 9] =  1.0;
			       }
 			    }
			    /* element strain (scalar) */
			    E->strain = (float *)calloc((E->lmesh.nno + 1), sizeof(float));

			  }
			  if(E->tracers_add_flavors){
			    /* nodal compotional flavors */
			    E->CF = (int **)safe_malloc( E->tracers_add_flavors * sizeof(int *));
			    /* maximum flavor value */
			    E->tmaxflavor = (int *)calloc(E->tracers_add_flavors,sizeof(int ));
			    for(j=0;j<E->tracers_add_flavors;j++)
			      E->CF[j] = (int *)calloc((E->lmesh.nno + 1), sizeof(int));
			    /* tracer flavor value */
			    E->tflavors = (int **)safe_malloc((E->advection.markers_uplimit + 1) * sizeof(int *));
			    for(j = 0; j < E->advection.markers_uplimit + 1;j++)
			      E->tflavors[j] = (int *)calloc(E->tracers_add_flavors,sizeof(int));
			  }
			}
		}
		force_report(E,"ok16b");
	}

	report(E, "convection, initial temperature");
#ifdef USE_GGRD
	force_report(E,"ok16c");
	convection_initial_temperature_and_comp_ggrd(E);
	force_report(E,"ok16d1");
#else
	convection_initial_temperature(E);
	force_report(E,"ok16d2");
#endif
	return;
}

/* =========================================== */

void convection_boundary_conditions(struct All_variables *E)
{
	velocity_boundary_conditions(E);	/* universal */
	temperature_boundary_conditions(E);

	temperatures_conform_bcs(E);

	return;
}

/* ===============================
   Initialization of fields .....

   NOTE: 

   there's a different routine for ggrd handling in ggrd_handling
   which has differnet logic for marker init etc. the other one gets
   called when USE_GGRD is compiled in


   =============================== */

void convection_initial_temperature(struct All_variables *E)
{
	int ll, mm, i, j, k, p, node, ii;
	//double temp, temp1, temp2, temp3, base, radius, radius2;
	//FILE *fp;
	double x1, y1, z1, con, beta;

	//int noz2, nfz, in1, in2, in3, instance, nox, noy, noz;
	int noz2, nox, noy, noz;
	//char input_s[200], output_file[255, m);
	//float weight, para1, plate_velocity, delta_temp, age;

	//const int dims = E->mesh.nsd;

	noy = E->lmesh.noy;
	noz = E->lmesh.noz;
	nox = E->lmesh.nox;

	p = 0;

	if(E->tracers_add_flavors)
	  myerror("convection_initial_temperature: not set up for tracer flavors",E);

	if(E->control.restart == 0)
	{
		if(E->control.CART3D)
		{
			for(i = 1; i <= noy; i++)
				for(j = 1; j <= nox; j++)
					for(k = 1; k <= noz; k++)
					{
						node = k + (j - 1) * noz + (i - 1) * noz * nox;
						x1 = E->X[1][node];
						z1 = E->X[3][node];
						y1 = E->X[2][node];
						if(E->control.add_init_t_gradient) /* default gradient */
						  E->T[node] = 1 - z1;
						else
						  E->T[node] = 0;
						/* perturbation */
						E->T[node] += E->convection.perturb_mag[p] * sin(M_PI * (1.0 - z1)) * 
						  cos(E->convection.perturb_k[p] * M_PI * x1) * 
						  ((E->mesh.nsd != 3) ? 1.0 : cos(E->convection.perturb_k[p] * M_PI * y1));

//             E->T[node] = 1-z1 + E->convection.perturb_mag[p] *
//                           sin(M_PI*(1.0-z1))*
//                      cos(E->convection.perturb_k[p] * M_PI*y1);


//      E->T[node] = 1.0-z1 + E->convection.perturb_mag[p]*drand48();

						E->C[node] = 0;

						E->node[node] = E->node[node] | (INTX | INTZ | INTY);
					}
		}
		else if(E->control.Rsphere)
		{
			mm = E->convection.perturb_mm[0];
			ll = E->convection.perturb_ll[0];
			con = E->convection.perturb_mag[0];
			con = (E->mesh.noz - 1) / (E->sphere.ro - E->sphere.ri);
			beta = E->sphere.ri / (E->sphere.ri - E->sphere.ro);
			noz2 = (E->mesh.noz - 1) / 2 + 1;

			for(i = 1; i <= noy; i++)
				for(j = 1; j <= nox; j++)
					for(k = 1; k <= noz; k++)
					{
						ii = k + E->lmesh.nzs - 1;
						node = k + (j - 1) * noz + (i - 1) * noz * nox;
						x1 = E->SX[1][node];
						z1 = E->SX[3][node];
						y1 = E->SX[2][node];
						/* those were overridden */
						//E->T[node] = 0.0;
						//E->T[node] = beta * (1.0 - 1.0 / z1) + E->convection.perturb_mag[p] * drand48();


/*             if (ii==noz2)    {
                  E->T[node] = con*modified_plgndr_a(ll,mm,x1)*cos(mm*y1);
                  }
             if (ii==noz2)   
               E->T[node] =
              0.01*modified_plgndr_a(ll,mm,x1);
 */
						
						//E->T[node] = beta * (1.0 - 1.0 / z1) + E->convection.perturb_mag[p] * modified_plgndr_a(ll, mm, x1) * cos(mm * y1) * sin(M_PI * (z1 - E->sphere.ri) / (E->sphere.ro - E->sphere.ri));
						if(E->control.add_init_t_gradient)
						  E->T[node] = beta * (1.0 - 1.0 / z1) ;
						else
						  E->T[node] = 0;
						E->T[node] += E->convection.perturb_mag[p] * sin(M_PI * (E->sphere.ro - z1) / (E->sphere.ro - E->sphere.ri)) * 
						  cos(E->convection.perturb_k[p] * M_PI * (x1 - E->XG1[1]) / (E->XG2[1] - E->XG1[1])) * 
						  ((E->mesh.nsd != 3) ? 1.0 : cos(E->convection.perturb_k[p] * M_PI * (y1 - E->XG1[2]) / (E->XG2[2] - E->XG1[2])));


						E->C[node] = 0.0;


						E->node[node] = E->node[node] | (INTX | INTZ | INTY);
					}
		}						// end for if else if of geometry

		if(E->control.composition){
		  if(E->control.composition_init_checkerboard)
		    convection_initial_markers(E,-1);
		  else if(E->control.cinit_sphere)
		    convection_initial_markers(E,-2);
		  else
		    convection_initial_markers(E,0);
		}

	}							// end for restart==0

	else if(E->control.restart){ /* restart branch */
	  
#ifdef USE_GZDIR
	  if(E->control.gzdir)
	    process_restart_tc_gzdir(E);
	  else{
	    if(E->tracers_track_strain)
	      force_report(E,"WARNING: strain init not implemented unless gzdir output is used");
	    process_restart_tc(E);
	  }
#else
	  if(E->tracers_track_strain)
	    force_report(E,"WARNING: strain init not implemented unless USE_GZDIR is used");
	  process_restart_tc(E);
#endif
	}

	temperatures_conform_bcs(E);

/*
   sprintf(output_file,"%s.XandT.%d",E->control.data_file,E->parallel.me);
   if ( (fp = fopen(output_file,"w")) != NULL) {
     if (E->control.Rsphere)
      for (j=1;j<=E->lmesh.nno;j++)
          fprintf(fp,"X[%06d] = %.4e Z[%06d] = %.4e Y[%06d] = %.4e T[%06d] = %.4e \n",j,E->SX[1][j],j,E->SX[2][j],j,E->SX[3][j],j,E->T[j]);
     else 
      for (j=1;j<=E->lmesh.nno;j++)
          fprintf(fp,"X[%06d] = %.4e Z[%06d] = %.4e Y[%06d] = %.4e T[%06d] = %.4e \n",j,E->X[1][j],j,E->X[2][j],j,E->X[3][j],j,E->T[j]);
      }        

   fclose(fp);

   exit(11); 
*/

	thermal_buoyancy(E);

	return;
}

void process_restart_tc(struct All_variables *E)
{

	int node, i, j, k, p;
	FILE *fp;
	float temp1, temp2, *temp;
	char input_s[200], output_file[255];

	temp = (float *)safe_malloc((E->mesh.noz + 1) * sizeof(float));

	if(E->control.restart == 1 || E->control.restart == 3)
	{
		sprintf(output_file, "%s.temp.%d.%d", E->convection.old_T_file, E->parallel.me, E->monitor.solution_cycles);
		fp = fopen(output_file, "r");
		fgets(input_s, 200, fp);
		sscanf(input_s, "%d %d %g", &i, &j, &E->monitor.elapsed_time);

		for(node = 1; node <= E->lmesh.nno; node++)
		{
			fgets(input_s, 200, fp);
			sscanf(input_s, "%g", &E->T[node]);
//             E->T[node] = min(1.0,E->T[node]);
			E->T[node] = max(0.0, E->T[node]);
		
			E->node[node] = E->node[node] | (INTX | INTZ | INTY);
		}
		fclose(fp);
	}
	else if(E->control.restart == -1)
	{
		sprintf(output_file, "%s.temp.%d.%d", E->convection.old_T_file, E->parallel.me, E->monitor.solution_cycles);
		fp = fopen(output_file, "r");
		fgets(input_s, 200, fp);
		sscanf(input_s, "%d %d %g", &i, &j, &E->monitor.elapsed_time);

		for(node = 1; node <= E->lmesh.nno; node++)
		{
			fgets(input_s, 200, fp);
			sscanf(input_s, "%g %g %g %g", &E->T[node], &temp1, &temp2, &E->C[node]);
//             E->T[node] = min(1.0,E->T[node]);
			E->T[node] = max(0.0, E->T[node]);
		
			E->node[node] = E->node[node] | (INTX | INTZ | INTY);
		}
		fclose(fp);
	}
	else if(E->control.restart == 2)
	{
//    sprintf(output_file,"%s.ave.0.%d",E->convection.old_T_file,E->monitor.solution_cycles);
		sprintf(output_file, "%s.ave", E->control.data_file);
		fprintf(stderr, "%s %d\n", output_file, E->mesh.noz);
		fp = fopen(output_file, "r");
		fgets(input_s, 200, fp);
		E->monitor.solution_cycles = 0;
		for(node = 1; node <= E->mesh.noz; node++)
		{
			fgets(input_s, 200, fp);
			sscanf(input_s, "%g %g", &temp1, &temp[node]);
		}
		fclose(fp);
		for(k = 1; k <= E->lmesh.noz; k++)
		{
			i = k + E->lmesh.nzs - 1;
			E->Have.T[k] = temp[i];
			fprintf(E->fp, "ab %d %g\n", k, E->Have.T[k]);
			fflush(E->fp);
		}

		p = 0;
		for(i = 1; i <= E->lmesh.noy; i++)
			for(j = 1; j <= E->lmesh.nox; j++)
				for(k = 1; k <= E->lmesh.noz; k++)
				{
					node = k + (j - 1) * E->lmesh.noz + (i - 1) * E->lmesh.noz * E->lmesh.nox;
					E->T[node] = E->Have.T[k] + E->convection.perturb_mag[p] * drand48();
				
					
					E->node[node] = E->node[node] | (INTX | INTZ | INTY);
				}
	}

// for composition

	if(E->control.composition){
	  
	  if(E->control.restart == 1 || E->control.restart == 2)
	    {
	      convection_initial_markers(E,0);
	    }
	  
	  else if(E->control.restart == 3)
	    {
	      sprintf(output_file, "%s.comp.%d.%d", E->convection.old_T_file, E->parallel.me, E->monitor.solution_cycles);
	      fp = fopen(output_file, "r");
	      fgets(input_s, 200, fp);
	      sscanf(input_s, "%d %d %g", &i, &j, &E->monitor.elapsed_time);
	      
	      for(node = 1; node <= E->lmesh.nno; node++)
		{
		  fgets(input_s, 200, fp);
		  sscanf(input_s, "%g", &E->C[node]);
		}
	      fclose(fp);
	      
	      convection_initial_markers1(E);
	    }
	  
	  else if(E->control.restart == -1)
	    {
	      
	      sprintf(output_file, "%s.traces.%d", E->convection.old_T_file, E->parallel.me);
	      fp = fopen(output_file, "r");
	      fgets(input_s, 200, fp);
	      sscanf(input_s, "%d %d %g", &E->advection.markers, &j, &temp1);
	      for(i = 1; i <= E->advection.markers; i++)
		{
		  fgets(input_s, 200, fp);
		  sscanf(input_s, "%lf %lf %lf %d %d", &E->XMC[1][i], &E->XMC[2][i], &E->XMC[3][i], &E->CElement[i], &E->C12[i]);
		  if(E->XMC[3][i] < E->XP[3][1])
		    E->XMC[3][i] = E->XP[3][1];
		  else if(E->XMC[3][i] > E->XP[3][E->lmesh.noz])
		    E->XMC[3][i] = E->XP[3][E->lmesh.noz];
		  if(E->XMC[2][i] < E->XP[2][1])
		    E->XMC[2][i] = E->XP[2][1];
		  else if(E->XMC[2][i] > E->XP[2][E->lmesh.noy])
		    E->XMC[2][i] = E->XP[2][E->lmesh.noy];
		  if(E->XMC[1][i] < E->XP[1][1])
		    E->XMC[1][i] = E->XP[1][1];
		  else if(E->XMC[1][i] > E->XP[1][E->lmesh.nox])
		    E->XMC[1][i] = E->XP[1][E->lmesh.nox];
		}
	      for(i = 1; i <= E->lmesh.nel; i++)
		{
		  fgets(input_s, 200, fp);
		  sscanf(input_s, "%g", &E->CE[i]);
		}
	      fclose(fp);

	      //    get_C_from_markers(E,E->C);
	      
	    }
	  /* this will assign C=1 on top left side */
	  if(E->mesh.slab_influx_side_bc)
	    composition_apply_slab_influx_side_bc(E);
	}

	E->advection.timesteps = E->monitor.solution_cycles;

	return;
}


void convection_initial_markers1(struct All_variables *E)
{
	//int *element, el, i, j, k, p, node, ii, jj;
  int *element, el, j, node,el_node,itracer;
  float *element_strain;
  double x, y, z, r, t, f, dX[4];
  static int been_here = 0,tscol;
  const int dims = E->mesh.nsd;
  const int ends = enodes[dims];
  if(!been_here){
    tscol = (E->tracers_track_fse)?(10):(1);
    been_here = 1;
  }

  element = (int *)safe_malloc((E->lmesh.nel + 1) * sizeof(int));
  if(E->tracers_track_strain)
    element_strain = (float *)calloc((E->lmesh.nel + 1), sizeof(float));
  
  for(el = 1; el <= E->lmesh.nel; el++){
    /* assign composition and strain based on average over nodes
       within element */
    t = 0;
    for(j = 1; j <= ends; j++){
      el_node = E->ien[el].node[j];
      t += E->C[el_node];
      if(E->tracers_track_strain)
	element_strain[el] += E->strain[el_node];
    }
    t = t / ends;
    if(E->tracers_track_strain)
      element_strain[el] /= ends;
    
    E->CE[el] = t;
    element[el] = 0;
  }
  
  if(E->control.CART3D)		/* assign new tracer locations */
    {
      node = 0;
      do
	{
	  x = drand48() * (E->XG2[1] - E->XG1[1]);
	  y = drand48() * (E->XG2[2] - E->XG1[2]);
	  z = drand48() * (E->XG2[3] - E->XG1[3]);
	  
	  if((x >= E->XP[1][1] && x <= E->XP[1][E->lmesh.nox]) && (y >= E->XP[2][1] && y <= E->XP[2][E->lmesh.noy]) && (z >= E->XP[3][1] && z <= E->XP[3][E->lmesh.noz]))
	    {
	      node++;
	      E->XMC[1][node] = x;
	      E->XMC[2][node] = y;
	      E->XMC[3][node] = z;
	      
	      el = get_element(E, E->XMC[1][node], E->XMC[2][node], E->XMC[3][node], dX);
	      E->CElement[node] = el;
	      element[el]++;
	    }
	} while(node < E->advection.markers);
    }
  else if(E->control.Rsphere)
    {
      node = 0;
      do
	{
	  x = (drand48() - 0.5) * 2.0;
	  y = drand48();
	  //			y = (drand48() - 0.5) * 2.0;
	  z = (drand48() - 0.5) * 2.0;
	  r = sqrt(x * x + y * y + z * z);
	  t = acos(z / r);
	  f = myatan(y, x);
	  if((t >= E->XP[1][1] && t <= E->XP[1][E->lmesh.nox]) && (f >= E->XP[2][1] && f <= E->XP[2][E->lmesh.noy]) && (r >= E->XP[3][1] && r <= E->XP[3][E->lmesh.noz]))
	    {
	      node++;
	      E->XMC[1][node] = t;
	      E->XMC[2][node] = f;
	      E->XMC[3][node] = r;
	      el = get_element(E, E->XMC[1][node], E->XMC[2][node], E->XMC[3][node], dX);
	      E->CElement[node] = el;
	      element[el]++;
	    }
	} while(node < E->advection.markers);
    }
  
  for(itracer = 1; itracer <= E->advection.markers; itracer++){
      el = E->CElement[itracer];

      if(E->CE[el] < 0.5)
	E->C12[itracer] = 0;
      else
	E->C12[itracer] = 1;
      
      /*	  if (E->CE[el]<dx)
		  E->C12[itracer] = 0;
		  else if (j>=1) {
		  E->C12[itracer] = 0;
		  E->CE[el] +=dx;
		  }
		  else {
		  E->C12[itracer] = 1;
		  }
      */
      if(E->tracers_track_strain) /* this will smooth quite a bit */
	E->tracer_strain[itracer*tscol] = element_strain[el];
  }
  /* reassign to nodal values */
  get_C_from_markers(E, E->C);

  if(E->tracers_add_flavors)
    get_CF_from_markers(E,E->CF);

  if(E->tracers_track_strain){
    /* reassign (?) */
    get_strain_from_markers(E,E->strain);
    free(element_strain);
  }

  return;
}
/* 
   use_element_nodes_for_init_c: 
   -2: use sphere
   -1: use checkerboard
   0:  use depth
   1:  use the nodes to assign element properties, and from that tracer properties
   

*/
void convection_initial_markers(struct All_variables *E,
				int use_element_nodes_for_init_c)
{
  //int el, i, j, k, p, node, ii, jj;
  int el, ntracer,j,node,k,i;
  //double x, y, z, r, t, f, dX[4], dx, dr;
  double x, y, z, r, t, f, dX[4];
  //char input_s[100], output_file[255];
  //FILE *fp;
  float temp,frac,locx[3],locp[3],dist;

  const int dims = E->mesh.nsd;
  const int ends = enodes[dims];

  if(E->tracers_assign_dense_only){
    /* 
       assign only tracers within elements that are on average dense
       
       this is, in general, not a good idea, but i added it for
       testing purposes

    */
    ntracer=0;
    for(el = 1; el <= E->lmesh.nel; el++){
      /* get mean composition */
      for(temp=0.0,j = 1; j <= ends; j++)
	temp += E->C[E->ien[el].node[j]];
      temp /= ends;
      if(temp > 0.5){
	for(j=0;j < E->advection.markers_per_ele;j++){
	  ntracer++;

	  if(ntracer > E->advection.markers)
	    myerror("only dense assign: out of markers",E);
	  /* get a random location within the element */
	  if(E->control.CART3D){ /* cartesian */
	    x = y = z = 0;
	    for(k=1;k <= ends;k++){
	      node = E->ien[el].node[k];
	      frac = drand48();
	      x += frac * E->X[1][node];
	      y += frac * E->X[2][node];
	      z += frac * E->X[3][node];
	    }
	    E->XMC[1][ntracer] = x / (float)ends;
	    E->XMC[2][ntracer] = y / (float)ends;
	    E->XMC[3][ntracer] = z / (float)ends;
	  }else{
	    x = y = z = 0;
	    for(k=1;k <= ends;k++){
	      node = E->ien[el].node[k];
	      frac = drand48();
	      rtp2xyz((float)E->SX[3][node],(float)E->SX[1][node],(float)E->SX[2][node],locx); /* convert to cart */
	      x += frac * locx[0];
	      y += frac * locx[1];
	      z += frac * locx[2];
	    }
	    locx[0] = x / (float)ends; locx[1] = y /(float)ends; locx[2] = z/(float)ends; 
	    xyz2rtp(locx[0],locx[1],locx[2],locp); /* back to spherical */
	    E->XMC[1][ntracer] = locp[1]; /* theta */
	    E->XMC[2][ntracer] = locp[2]; /* phi */
	    E->XMC[3][ntracer] = locp[0]; /* r */
	  }
	  E->C12[ntracer] = 1;
	  E->CElement[ntracer] = el;
	}
      }
    }

  }else{
    /* regular operation */
    
	      
    if(E->control.CART3D)
      {
	ntracer = 0;
	do
	  {
	    x = drand48() * (E->XG2[1] - E->XG1[1]);
	    y = drand48() * (E->XG2[2] - E->XG1[2]);
	    z = drand48() * (E->XG2[3] - E->XG1[3]);
		      
	    if((x >= E->XP[1][1] && x <= E->XP[1][E->lmesh.nox]) && (y >= E->XP[2][1] && y <= E->XP[2][E->lmesh.noy]) && (z >= E->XP[3][1] && z <= E->XP[3][E->lmesh.noz]))
	      {
		ntracer++;
		E->XMC[1][ntracer] = x;
		E->XMC[2][ntracer] = y;
		E->XMC[3][ntracer] = z;
			  
		el = get_element(E, E->XMC[1][ntracer], E->XMC[2][ntracer], E->XMC[3][ntracer], dX);
		E->CElement[ntracer] = el;
		switch(use_element_nodes_for_init_c){
		case 1: 	/* from nodes */
		  for(temp=0.0,j = 1; j <= ends; j++){
		    temp += E->C[E->ien[el].node[j]];
		  }
		  temp /= ends;
		  if(temp >= 0.5)
		    E->C12[ntracer] = 1;
		  else
		    E->C12[ntracer] = 0;
		  //fprintf(stderr,"%g %i\n",temp,E->C12[ntracer]);
		  break;
		case 0:		/* based on depth */
		  if(E->XMC[3][ntracer] > E->viscosity.zcomp)
		    E->C12[ntracer] = 0;
		  else
		    E->C12[ntracer] = 1;
		  break;
		case -1:	/* checker board with x and z */
		  i = (int)(E->XMC[1][ntracer]/0.2+0.5);
		  k = (int)(E->XMC[3][ntracer]/0.2+0.5);
		  if(i%2==0){
		    if(k%2==0)
		      E->C12[ntracer] = 1;
		    else
		      E->C12[ntracer] = 0;
		  }else{
		    if(k%2==0)
		      E->C12[ntracer] = 0;
		    else
		      E->C12[ntracer] = 1;
		  }
		  break;
		case -2:	/* from sphere */
		  dist  = pow(E->XMC[1][ntracer] - E->control.cinit_sphere_par[0],2);
		  dist += pow(E->XMC[2][ntracer] - E->control.cinit_sphere_par[1],2);
		  dist += pow(E->XMC[3][ntracer] - E->control.cinit_sphere_par[2],2);
		  dist = sqrt(dist);
		  if(dist <= E->control.cinit_sphere_par[3])
		    E->C12[ntracer] = E->control.cinit_sphere_par[4];
		  else
		    E->C12[ntracer] = E->control.cinit_sphere_par[5];
		  break;
		default:
		  myerror("use_element_nodes_for_init_c out of bounds in convection_initial_markers for Cartesian",E);
		}
	      }
	  } while(ntracer < E->advection.markers);
      }
    else if(E->control.Rsphere)
      {
	ntracer = 0;
	do
	  {
	    x = (drand48() - 0.5) * 2.0;
	    y = drand48();
	    //       y = (drand48()-0.5)*2.0;
	    z = (drand48() - 0.5) * 2.0;
	    r = sqrt(x * x + y * y + z * z);
	    t = acos(z / r);
	    f = myatan(y, x);
	    if((t >= E->XP[1][1] && t <= E->XP[1][E->lmesh.nox]) && (f >= E->XP[2][1] && f <= E->XP[2][E->lmesh.noy]) && (r >= E->XP[3][1] && r <= E->XP[3][E->lmesh.noz]))
	      {
		ntracer++;
		E->XMC[1][ntracer] = t;
		E->XMC[2][ntracer] = f;
		E->XMC[3][ntracer] = r;
		el = get_element(E, E->XMC[1][ntracer], E->XMC[2][ntracer], E->XMC[3][ntracer], dX);
		E->CElement[ntracer] = el;

		switch(use_element_nodes_for_init_c){
		case 1:		/* based on nodes */
		  for(temp=0.0,j = 1; j <= ends; j++)
		    temp += E->C[E->ien[el].node[j]];
		  temp /= ends;
		  if(temp >0.5)
		    E->C12[ntracer] = 1;
		  else
		    E->C12[ntracer] = 0;
		  break;
		case 0:		/* based on depth */
		  if(r > E->viscosity.zcomp)
		    E->C12[ntracer] = 0;
		  else
		    E->C12[ntracer] = 1;
		  break;
		case -1:
		case -2:
		  myerror("checkerboard or sphere not yet implemented for spherical",E);
		  break;
		default:
		  myerror("use_element_nodes_for_init_c out of bounds for spherical in convection_initial_markers",E);
		}
			
			
	      }
	  } while(ntracer < E->advection.markers);
      }
  } /* end regular operation */
	
    /* assign tracers based on ggrd */
  if(E->tracers_add_flavors){
#ifdef USE_GGRD
    assign_flavor_to_tracer_from_grd(E);
#else
    myerror("convection_initial_markers: flavor init requires GGRD compilation",E);
#endif
  }
  /* 
     get nodal values, reassign to nodes
  */
  get_C_from_markers(E, E->C);
  if(E->tracers_add_flavors)
    get_CF_from_markers(E,E->CF);
  return;
}
/* 
	   
assign a flavor to this tracer based on proximity to a node

*/
void assign_flavor_to_tracer_based_on_node(struct All_variables *E, 
					   int ntracer, 
					   int use_element_nodes_for_init_c)
{
  int j,el,i;
  const int dims = E->mesh.nsd;
  const int ends = enodes[dims];
  float dmin,dist;
  int nmin,lnode;

  el = E->CElement[ntracer];	/* element we're in */
  switch(use_element_nodes_for_init_c){
  case 1:
    for(dmin = 1e20,j = 1; j <= ends; j++){
      lnode = E->ien[el].node[j];
      dist = distance_to_node(E->XMC[1][ntracer],E->XMC[2][ntracer],E->XMC[3][ntracer],E,lnode);
      if(dist < dmin){		/* pick the node that is closest to the tracer */
	dmin = dist;
	nmin = lnode;
      }
    }
    for(i=0;i < E->tracers_add_flavors;i++)
      E->tflavors[ntracer][i] = E->CF[i][lnode];
    break;
  case 0:
    for(i=0;i < E->tracers_add_flavors;i++){
      if(E->XMC[3][ntracer] > E->viscosity.zcomp)
	E->tflavors[ntracer][i] = 0;
      else
	E->tflavors[ntracer][i] = 1;
    }
    break;
  default:
    myerror("use_element_nodes_for_init_c out of bounds for flavors",E);
    
  }
}

void setup_plume_problem(struct All_variables *E)
{
	int i;
	//FILE *fp;
	//char output_file[255];

	int l, noz;
	double temp, temp1;

	noz = E->lmesh.noz;


	E->control.plate_vel = E->control.plate_vel * 0.01 / 3.1536e7;
	/* in m/sec */
	E->control.plate_vel = E->control.plate_vel / E->monitor.velo_scale;
	/* dimensionless */

	/* in sec */

	E->control.plate_age = E->control.plate_age * 1.0e6 * 3.1536e7;
	/* in sec */
	E->control.plate_age = E->control.plate_age / E->monitor.time_scale;
	/* dimensionless */


	for(i = 1; i <= noz; i++)
	{
		temp = 0.5 * E->X[3][i] / sqrt(E->control.plate_age);
		E->segment.Tz[i] = erf(temp);
	}

	temp = 0.0;
	for(i = 1; i <= noz; i++)
	{
		l = E->mat[i];
		if(E->viscosity.RHEOL == 1)
			E->Have.Vi[i] = E->viscosity.N0[l - 1] * exp(E->viscosity.E[l - 1] / (E->viscosity.T[l - 1] + E->segment.Tz[i]));
		else if(E->viscosity.RHEOL == 2)
			E->Have.Vi[i] = E->viscosity.N0[l - 1] * exp((E->viscosity.E[l - 1] + (1 - E->X[3][i]) * E->viscosity.Z[l - 1]) / (E->viscosity.T[l - 1] + E->segment.Tz[i]));
		else if(E->viscosity.RHEOL == 3)
			E->Have.Vi[i] = E->viscosity.N0[l - 1] * exp((E->viscosity.E[l - 1] + (1 - E->X[3][i]) * E->viscosity.Z[l - 1]) / E->segment.Tz[i] - (E->viscosity.E[l - 1] + E->viscosity.Z[l - 1]));
		if(i > 1)
		{
			temp += (1 / E->Have.Vi[i] + 1 / E->Have.Vi[i - 1]) * (E->X[3][i] - E->X[3][i - 1]) * 0.5;
		}
	}
	temp = E->control.plate_vel / temp;

	temp1 = 0.0;
	for(i = 1; i <= noz; i++)
	{
		if(i > 1)
		{
			temp1 += (1 / E->Have.Vi[i] + 1 / E->Have.Vi[i - 1]) * (E->X[3][i] - E->X[3][i - 1]) * 0.5;
		}
		E->segment.Vx[i] = -temp * temp1 + E->control.plate_vel;
		fprintf(E->fp, "visc %d %g %g %g\n", i, E->Have.Vi[i], E->segment.Tz[i], E->segment.Vx[i]);
	}

	/* non-dimensionalize other parameters */
	E->segment.plume_radius /= E->monitor.length_scale;
	E->segment.plume_DT /= E->data.ref_temperature;
	E->segment.plume_coord[1] /= E->monitor.length_scale;
	if(E->mesh.nsd == 3)
		E->segment.plume_coord[3] /= E->monitor.length_scale;

	return;
}



void PG_process(struct All_variables *E, int ii)
{
  float *P, *P2,*fdummy;
	//float visc[9];

	//int i, j, k, p, a1, nint, n, el;
	int i, j, n, el;
	//int node, hnode;
	//int this_node, that_node;

	//struct Shape_function GN;
	//struct Shape_function_dx GNx;
	//struct Shape_function_dA dOmega;

	const int vpts = vpoints[E->mesh.nsd];
	const int ends = enodes[E->mesh.nsd];

	//float x1, x2, int1, int2;
	float int1, int2;

	P = (float *)safe_malloc((1 + E->lmesh.nno) * sizeof(double));
	P2 = (float *)safe_malloc((1 + E->lmesh.nel) * vpts * sizeof(double));

	/* This is an ideal point to calculate the energy consistency integral that
	 * Slava suggested to me.. NOTE, the buoyancy and velocity are most closely in step
	 * at this point, before advection. */

	strain_rate_2_inv(E, P2, 0,0,fdummy);	/* strain rate invariant squared */
	v_to_nodes(E, P2, P, E->mesh.levmax);

	int1 = 0.0;
	int2 = 0.0;

	for(el = 1; el <= E->lmesh.nel; el++)
	{

		for(i = 1; i <= ends; i++)
		{
			n = E->ien[el].node[i];
			for(j = 1; j <= vpts; j++)
			{
				int1 += E->EVi[(el - 1) * vpts + j] * P[n] * E->N.vpt[GNVINDEX(i, j)] * E->gDA[el].vpt[j];
				int2 += E->V[3][n] * E->buoyancy[n] * E->N.vpt[GNVINDEX(i, j)] * E->gDA[el].vpt[j];
			}
		}
	}


	if(int1 != 0.0)
		fprintf(E->fp, "Energy balance integral accurate to  %g%% in %e\n", 100.0 * fabs((int1 - int2) / int1), int1);


	free((void *)P);
	free((void *)P2);
	return;
}

/* 
   assign C=1 composition at left half of box, on top 
*/
void composition_apply_slab_influx_side_bc(struct All_variables *E)
{
  int i;
  if(E->control.composition){
    for(i=1;i<=E->advection.markers;i++){
      if((E->XMC[1][i] <= 0.2)&&
	 (E->XMC[2][i] <= E->mesh.slab_influx_y2)&&
	 (E->XMC[3][i] >= E->mesh.slab_influx_z2)){
	E->C12[i] = 1;
      }
    }
  }

}
