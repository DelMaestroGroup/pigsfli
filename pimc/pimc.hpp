//
//  pimc.hpp
//  pimc
//
//  Created by Emanuel Casiano-Diaz on 8/22/20.
//  Copyright © 2020 Emanuel Casiano-Díaz. All rights reserved.
//

#ifndef pimc_hpp
#define pimc_hpp

#include <stdio.h>
#include<iostream>
#include<vector>
#include<boost/random.hpp>
#include<cmath>
#include<chrono>
#include<iomanip>  // for std::setprecision
#include <fstream>
#include <cstdlib> // for exit function

using namespace std;
using namespace std::chrono;

/*--------------------------- Class Definitions ------------------------------*/

class Kink
{
    public:
    // Attribute declarations
    double tau;
    int n,src,dest,prev,next,src_replica,dest_replica;
    
    // Member function declarations (prototypes)
    Kink (double,int,int,int,int,int,int,int); // Kink constructor
    
    // Make "<<" a friend of the Kink class
    friend ostream& operator<<(ostream& os, const Kink& dt);
};

// Member function definitions
Kink::Kink (double time,int particles,int source_site,int destination_site,
            int prev_kink_idx,int next_kink_idx,
            int source_replica,int destination_replica){
    tau = time;
    n = particles;
    src = source_site;
    dest = destination_site;
    prev = prev_kink_idx;
    next = next_kink_idx;
    src_replica = source_replica;
    dest_replica = destination_replica;
    // bool swap_kink -> Add this
    // int replica_idx
}

// Overload "<<" operator
ostream& operator<<(ostream& os, const Kink& dt)
{
    os << '<' << dt.tau << ',' << dt.n << ',' << dt.src << ','
    << dt.dest << ',' << dt.prev << ',' << dt.next <<'>'
    << dt.src_replica << dt.dest_replica;

    return os;
}

/*-------------------------- Function Definitions ----------------------------*/

vector<int> random_boson_config(int M,int N,boost::random::mt19937 &rng){
    // Generates random Fock state of N bosons in M=L^D sites
    
    vector<int> alpha (M,0);
    int src;
    
    // Initialize the distribution object. Note: Support is fully closed [0,M-1]
    boost::random::uniform_int_distribution<> sites(0, M-1);
    
    // Randomly sprinkle the N particles among sites
    for (int n=1; n<=N; n++){
        src = sites(rng);
        alpha[src] += 1;
    }
    return alpha;
}

/*----------------------------------------------------------------------------*/

vector<Kink> create_paths(vector<int> &fock_state,int M,int replica_idx){

    // Pre-allocate kinks. Recall: (tau,n,src,dest,prev,next)
    vector<Kink> paths(10000000,Kink(-1.0,-1,-1,-1,-1,-1,-1,-1));

    // Initialize the first M=L^D kinks
    for (int site=0; site<M; site++){
        paths[site] = Kink(0.0,fock_state[site],site,site,-1,-1,
                           replica_idx,replica_idx);
    }
    return paths;
}

/*----------------------------------------------------------------------------*/

// Create function that calculates vector norm given array
double norm(vector<double> point){
    
    double squared_sum=0;
        
    for (int i=0; i<point.size(); i++){
        squared_sum += point[i]*point[i];
    }
    
    return sqrt(squared_sum);
}

/*----------------------------------------------------------------------------*/

void build_hypercube_adjacency_matrix(int L,int D, string boundary_condition,
                                      vector<vector<int>> &adjacency_matrix){
    
    int M = pow(L,D);
    int site_left,site_right,site_up,site_down,site_high,site_low;
    int top_row_ctr,bottom_row_ctr;
    
    vector<int> rows (D*2,0);
    
    top_row_ctr=L;
    bottom_row_ctr=0;
    
    // Initialize adjacency matrix with placeholder zeroes
    for (int i=0; i<M; i++){adjacency_matrix.push_back(rows);}
    
    for (int site=0; site<M; site++){
        
        // Left neighbor
        if (site%L==0){site_left=site+(L-1);}
        else {site_left=site-1;}
        
        // Right neighbor
        if ((site+1)%L==0){site_right=site-(L-1);}
        else {site_right=site+1;}
        
        // Top neighbor
        if (site%(L*L)==0){top_row_ctr=L;}
        if (top_row_ctr>0){
            site_up = site+L*(L-1);
            top_row_ctr-=1;
        }
        else {site_up=site-L;}

        // Bottom neighbor
        if ((site-L*(L-1))%(L*L)==0){bottom_row_ctr=L;}
        if (bottom_row_ctr>0){
            site_down = site-L*(L-1);
            bottom_row_ctr-=1;
        }
        else {site_down=site+L;}
        
        // High neighbor (i.e, layer above in 3D; ran out of words, sorry)
        if (site-L*L<0){site_high=site+L*L*(L-1);}
        else {site_high=site-L*L;}
        
        // Low neighbor (i.e, layer below in 3D; ran out of words, sorry)
        if (site+L*L>L*L*L-1){site_low=site-L*L*(L-1);}
        else {site_low=site+L*L;}
        
        // Populate the adjacency matrix (in compact form)
        if (L<2){
            cout << "ERROR: CODE DOES NOT SUPPORT L<2 AT THE MOMENT" << endl;
        }
        else{
            adjacency_matrix[site][0]=site_left;
            adjacency_matrix[site][1]=site_right;
            if (D>1){
                adjacency_matrix[site][2]=site_up;
                adjacency_matrix[site][3]=site_down;
            }
            if (D>2){
                adjacency_matrix[site][4]=site_high;
                adjacency_matrix[site][5]=site_low;
            }
        }
    }
    return;
}
    
/*----------------------------------------------------------------------------*/

void build_adjacency_matrix(int L,int D,string boundary_condition,
                            vector<vector<bool>>&adjacency_matrix){

    // Variable declarations
    int M = pow(L,D); // Number of lattice points
    int ctr,a1,a2,a3;
    double r_NN;
    vector<double> points_difference (D,0.0);

    // Initialize normalized basis vectors
    vector<double> a1_vec {1.0,0.0,0.0};
    vector<double> a2_vec {0.0,1.0,0.0};
    vector<double> a3_vec {0.0,0.0,1.0};
    
    // Initialize array that will store all the points
    vector<double> empty_point (D,0);
    vector<vector<double>> points (M,empty_point);
    
    // Norm of basis vectors
    a1 = norm(a1_vec);
    a2 = norm(a2_vec);
    a3 = norm(a3_vec);
        
    // Build the lattice vectors
    ctr = 0;
    if (D==1){
        for (int i1=0; i1<L; i1++){
            points[ctr][0] = i1*a1;
            ctr++;
        }
    }
    else if (D==2){
        for (int i1=0; i1<L; i1++){
            for (int i2=0; i2<L; i2++){
                points[ctr][0] = i1*a1;
                points[ctr][1] = i2*a2;
                ctr++;
            }
        }
    }
    else{ // D==3
        for (int i1=0; i1<L; i1++){
            for (int i2=0; i2<L; i2++){
                for (int i3=0; i3<L; i3++){
                    points[ctr][0] = i1*a1;
                    points[ctr][1] = i2*a2;
                    points[ctr][2] = i3*a3;
                    ctr++;
                }
            }
        }
    }
    
    // Set the nearest-neighbor distance
    r_NN = a1;
    
    // Set adjacency matrix elements by comparing inter-site distances
    for (int i=0; i<M; i++){
        for (int j=i+1; j<M; j++){
            for (int axis=0; axis<D; axis++){
                points_difference[axis] = points[i][axis]-points[j][axis];
            }
            if (boundary_condition=="pbc"){
                adjacency_matrix[i][j] = norm(points_difference) <= r_NN
                || norm(points_difference) == L-1;
            }
            else if (boundary_condition=="obc"){
                adjacency_matrix[i][j] = norm(points_difference) <= r_NN;
            }
            else{
                cout << "ERROR: Boundary condition not supported" << endl;
            }
            // Fill out the symmetric elements
            adjacency_matrix[j][i] = adjacency_matrix[i][j];
        }
    }
    
    return;
 }

/*----------------------------------------------------------------------------*/

void create_sub_sites(vector<int> &sub_sites,int l_A,int L,int D,int M){
    // Hard coded to cluster of sites in 1D and to SQUARE region in 2D, for now.
    // Doesn't work for 3D yet.
    
    int m_A,ctr;
    
    m_A = pow(l_A,D); // total subsystem sites
    ctr = 0;
    
    if (L>=2 && l_A>=L){cout << "ERROR: L needs to be larger than l_A" << endl;}
    
    if (D==1 || L==2){ // cluster
        for (int i=0; i<l_A; i++){sub_sites.push_back(i);}
    }
    else if (D==2){
        for (int i=0; i<M; i++){
            if (sub_sites.size()==m_A){break;}
            sub_sites.push_back(i);
            ctr+=1;
            if (ctr==l_A){ctr=0;i+=(L-l_A);}
        }
    }
    else if (D==3){
        cout << "ERROR: create_sub_sites does not support 3D yet" << endl;
    }
    else{
        // lol
    }
    cout << endl;
    
    return;
}
/*----------------------------------------------------------------------------*/

void insert_worm(vector<Kink> &paths, int &num_kinks, int &head_idx,
                 int &tail_idx, int M, int N, double U, double mu, double t,
                 double beta, double eta, bool canonical, double &N_tracker,
                 int &N_zero, int &N_beta, vector<int> &last_kinks,
                 unsigned long long int &insert_worm_attempts,
                 unsigned long long int &insert_worm_accepts,
                 unsigned long long int &insert_anti_attempts,
                 unsigned long long int &insert_anti_accepts,
                 boost::random::mt19937 &rng){
    
    // Variable declarations
    int k,n,src,dest,prev,next,n_head,n_tail,src_replica,dest_replica;
    double tau,tau_h,tau_t,tau_prev,tau_next,tau_flat,l_path,dN,dV,p_iw,p_dw,R;
    bool is_worm;
    
    // Can only perform update if there are no worm ends
    if (head_idx != -1 || tail_idx != -1){return;}
    
    // Randomly sample a flat interval (or kink if you like)
    boost::random::uniform_int_distribution<> flats(0, num_kinks-1);
    k = flats(rng);
    
    // Extract the attributes of the kink at the bottom of the flat interval
    tau = paths[k].tau;
    n = paths[k].n;
    src = paths[k].src;
    dest = paths[k].dest;
    prev = paths[k].prev;
    next = paths[k].next;
    src_replica = paths[k].src_replica;
    dest_replica = paths[k].dest_replica;
    
    // Calculate the length of the flat interval
    tau_prev = tau;
    if (next != -1) // tau_next extractable iff sampled kink is not the last
        tau_next = paths[next].tau;
    else
        tau_next = beta;
    tau_flat = tau_next - tau_prev;
    
    // Randomly choose where to insert worm ends in the flat interval
    boost::random::uniform_real_distribution<double> rnum(0.0, 1.0);
    tau_h = tau_prev + tau_flat*rnum(rng);
    tau_t = tau_prev + tau_flat*rnum(rng);
    
    // Based on worm end time, determine worm type: antiworm or worm
    if (tau_h > tau_t){
        is_worm = true;
        insert_worm_attempts += 1; // Attempts counter
    }
    else{
        is_worm = false;
        insert_anti_attempts += 1;
    }
    
    // Determine the no. of particles after each worm end
    if (is_worm){
        n_tail = n + 1;
        n_head = n;
    }
    else{
        n_tail = n;
        n_head = n - 1;
    }
    
    // Reject update if illegal worm insertion is proposed
    if (n == 0 && !(is_worm)){insert_anti_attempts-=1;return;}
    if (tau_h == tau_prev || tau_t == tau_prev){return;}
    if (tau_h == tau_t){return;}
    
    // Determine length of modified path and particle change
    l_path = tau_h - tau_t;
    dN = l_path/beta;
    
    // Canonical simulations: Restrict updates to interval N:(N-1,N+1)
    if (canonical)
        if ((N_tracker+dN) < (N-1) || (N_tracker+dN) > (N+1)){return;}
    
    // Calculate the difference in diagonal energy dV = \epsilon_w - \epsilon
    dV = (U/2.0)*(n_tail*(n_tail-1)-n_head*(n_head-1)) - mu*(n_tail-n_head);
    
    // Build the Metropolis ratio (R)
    p_dw = 0.5;
    p_iw = 0.5;
    R = eta * eta * n_tail * exp(-dV*(tau_h-tau_t))* (p_dw/p_iw) *
    num_kinks * tau_flat * tau_flat;

    // Metropolis sampling
    if (rnum(rng) < R){ // Accept

        // Activate the first two available kinks
        if (is_worm){
            paths[num_kinks]=Kink(tau_t,n_tail,src,src,k,num_kinks+1,
                                  src_replica,dest_replica);
            paths[num_kinks+1]=Kink(tau_h,n_head,src,src,num_kinks,next,
                                    src_replica,dest_replica);
            
            // Save indices of head & tail kinks
            head_idx = num_kinks + 1;
            tail_idx = num_kinks;
            
            // Add to Acceptance counter
            insert_worm_accepts += 1;
        }
        else{ // Antiworm
            paths[num_kinks]=Kink(tau_h,n_head,src,src,k,num_kinks+1,
                                  src_replica,dest_replica);
            paths[num_kinks+1]=Kink(tau_t,n_tail,src,src,num_kinks,next,
                                    src_replica,dest_replica);
            
            // Save indices of head & tail kinks
            head_idx = num_kinks;
            tail_idx = num_kinks+1;
            
            // Add to Acceptance counter
            insert_anti_accepts += 1;
        }
        
        // "Connect" next of lower bound kink to nearest worm end
        paths[k].next = num_kinks;
        
        // "Connect" prev of next kink to nearest worm end
        if(next!=-1){paths[next].prev = num_kinks+1;}
        
        // Update trackers for: no of active kinks, total particles
        num_kinks += 2;
        N_tracker += dN;
        
        // If later worm end is last kink on site, update last kinks tracker vec
        if (next==-1){
            if (is_worm){last_kinks[src]=head_idx;}
            else {last_kinks[src]=tail_idx;}
        }
        
        return;
    }
    else // Reject
        return;
}

/*----------------------------------------------------------------------------*/

void delete_worm(vector<Kink> &paths, int &num_kinks, int &head_idx,
                 int &tail_idx, int M, int N, double U, double mu, double t,
                 double beta, double eta, bool canonical, double &N_tracker,
                 int &N_zero, int &N_beta, vector<int> &last_kinks,
                 unsigned long long int &delete_worm_attempts,
                 unsigned long long int &delete_worm_accepts,
                 unsigned long long int &delete_anti_attempts,
                 unsigned long long int &delete_anti_accepts,
                 boost::random::mt19937 &rng){
    
    // Variable declarations
    int n,src,dest,prev,next,n_head,n_tail;
    int prev_h,next_h,prev_t,next_t,high_end,low_end;
    double tau_h,tau_t,tau_prev,tau_next,tau_flat,l_path,dN,dV,p_iw,p_dw,R;
    bool is_worm;
    
    // Can only propose worm deletion if both worm ends are present
    if (head_idx == -1 || tail_idx == -1){return;}
    
    // Can only delete worm if wormends are on same flat interval
    if (head_idx != paths[tail_idx].prev &&
        tail_idx != paths[head_idx].prev)
        return;
    
    // Extract worm end attributes
    tau_h = paths[head_idx].tau; // Head attributes
    n_head = paths[head_idx].n;
    src = paths[head_idx].src;
    dest = paths[head_idx].dest;
    prev_h = paths[head_idx].prev;
    next_h = paths[head_idx].next;
    
    tau_t = paths[tail_idx].tau; // Tail attributes
    n_tail = paths[tail_idx].n;
    src = paths[tail_idx].src;
    dest = paths[tail_idx].dest;
    prev_t = paths[tail_idx].prev;
    next_t = paths[tail_idx].next;

    // Identify the type of worm
    if (tau_h > tau_t)
        is_worm = true;
    else
        is_worm = false; // antiworm
    
    // Identify lower and upper bound of flat interval where worm lives
    if(is_worm){
        tau_prev = paths[prev_t].tau;
        delete_worm_attempts += 1; // Attempts counter
        if(paths[head_idx].next == -1)
            tau_next = beta;
        else
            tau_next = paths[next_h].tau;
        n = paths[prev_t].n; // particles originally in the flat
            }
    else{ // antiworm
        tau_prev = paths[prev_h].tau;
        delete_anti_attempts += 1; // Attempts counter
        if (paths[tail_idx].next == -1)
            tau_next = beta;
        else
            tau_next = paths[next_t].tau;
        n = paths[prev_h].n;
            }
    
    // Calculate the length of the flat interval
    tau_flat = tau_next - tau_prev;
    
    // Define upper,lower index variables independent of worm type
    if (is_worm){
        next = next_h;
        prev = prev_t;
        high_end = head_idx;
        low_end = tail_idx;
    }
    else{
        next = next_t;
        prev = prev_h;
        high_end = tail_idx;
        low_end = head_idx;
    }
    
    // Determine length of modified path and particle change
    l_path = tau_h-tau_t;
    dN = -l_path/beta;
    
    // Canonical simulations: Restrict updates to interval N:(N-1,N+1)
    if (canonical)
        if ((N_tracker+dN) < (N-1) || (N_tracker+dN) > (N+1)){return;}
        
    // Calculate the difference in diagonal energy dV = \epsilon_w - \epsilon
    dV = (U/2.0)*(n_tail*(n_tail-1)-n_head*(n_head-1)) - mu*(n_tail-n_head);
    
    // Build the Metropolis ratio (R)
    p_dw = 1.0;
    p_iw = 1.0;
    R = (eta*eta) * n_tail * exp(-dV*(tau_h-tau_t))* (p_dw/p_iw) *
    (num_kinks-2) * (tau_flat*tau_flat);
    R = 1.0/R;
    
    // Metropolis sampling
    boost::random::uniform_real_distribution<double> rnum(0.0, 1.0);
    if (rnum(rng) < R){ // Accept
        
        // Add to Acceptance counter
        if (is_worm)
            delete_worm_accepts += 1;
        else
            delete_anti_accepts += 1;
        
        // Stage 1: Delete the higher worm end
        
        // num_kinks-1 will be swapped. Modify links to these.
        if (paths[num_kinks-1].next!=-1)
            paths[paths[num_kinks-1].next].prev = high_end;
        paths[paths[num_kinks-1].prev].next = high_end;
                
        swap(paths[high_end],paths[num_kinks-1]);
        
        // Upper,lower bounds of flat could've been swapped. Correct if so.
        if (prev==num_kinks-1){prev=high_end;}
        else if (next==num_kinks-1){next=high_end;}
        else if (low_end==num_kinks-1){low_end=high_end;}
        else {;}
        
        // The swapped kink could've been the last on its site.
        if (paths[high_end].next==-1){
            last_kinks[paths[high_end].src]=high_end;
        }
        
        // Connect upper,lower bounds to lower worm end
        if (next!=-1)
            paths[next].prev = low_end;
        paths[low_end].next = next;
        
        if (next==-1){last_kinks[src]=low_end;}

        // Stage 2: Delete the lower worm end
        
        // num_kinks-2 will be swapped. Modify links to these.
        if (paths[num_kinks-2].next!=-1)
            paths[paths[num_kinks-2].next].prev = low_end;
        paths[paths[num_kinks-2].prev].next = low_end;
        
        swap(paths[low_end],paths[num_kinks-2]);

        if (prev==num_kinks-2){prev=low_end;}
        else if (next==num_kinks-2){next=low_end;}
        else {;}

        if (paths[low_end].next==-1){
            last_kinks[paths[low_end].src]=low_end;
        }
        
        if (next!=-1)
            paths[next].prev = prev;
        paths[prev].next = next;
        
        if (next==-1){last_kinks[src]=prev;}

        // Deactivate the head,tail indices
        head_idx = -1;
        tail_idx = -1;
        
        // Update trackers for: num of active kinks, total particles
        num_kinks -= 2;
        N_tracker += dN;
        
        return;
    }
        
    else // Reject
            return;
}

/*----------------------------------------------------------------------------*/

void insertZero(vector<Kink> &paths, int &num_kinks, int &head_idx,
                int &tail_idx, int M, int N, double U, double mu, double t,
                double beta, double eta, bool canonical, double &N_tracker,
                int &N_zero, int &N_beta, vector<int> &last_kinks,
                unsigned long long int &insertZero_worm_attempts,
                unsigned long long int &insertZero_worm_accepts,
                unsigned long long int &insertZero_anti_attempts,
                unsigned long long int &insertZero_anti_accepts,
                boost::random::mt19937 &rng){
    
    // Variable declarations
    int n,src,dest,prev,next,n_head,n_tail,i,N_b,src_replica,dest_replica;
    double tau_prev,tau_flat,l_path,dN,dV,R,p_type,tau_new,p_wormend,C,W,
    p_dz,p_iz;
    bool is_worm;

    // Cannot insert if there's two worm ends present
    if (head_idx != -1 and tail_idx != -1){return;}
        
    // Randomly select site on which to insert worm/antiworm from tau=0
    boost::random::uniform_int_distribution<> sites(0, M-1);
    i = sites(rng);
    
    // Extract attributes of insertion flat
    tau_prev = paths[i].tau; // tau is just zero
    n = paths[i].n;
    src = paths[i].src;
    dest = paths[i].dest;
    prev = paths[i].prev;
    next = paths[i].next;
    src_replica = paths[i].src;
    dest_replica = paths[i].dest_replica;
    
    // Determine the length of insertion flat interval
    if (next != -1)
        tau_flat = paths[next].tau;
    else
        tau_flat = beta;
    
    // Choose worm/antiworm insertion based on worm ends present
    boost::random::uniform_real_distribution<double> rnum(0.0, 1.0);
    if (head_idx==-1 and tail_idx==-1){ // no worm ends present
        if (n==0){ // can only insert worm, not antiworm
            is_worm = true;
            p_type = 1.0;
        }
        else{ // choose worm or antiworm insertion with equal probability
            if (rnum(rng) < 0.5)
                is_worm = true;
            else
                is_worm = false;
            p_type = 0.5;
        }
    }
    else if (head_idx!=-1){ // only worm head present, can insert antiworm only
        if (n==0){
            insertZero_anti_attempts += 1;
            return; // cannot insert proposed antiworm, no particles present
        }
        else{
            is_worm = false;
            p_type = 1.0;
        }
    }
    else{ // only tail present, can insert worm only
        is_worm = true;
        p_type = 1.0;
    }
    
    // Add to worm/antiworm insertion attempt counters
    if (is_worm){insertZero_worm_attempts += 1;}
    else {insertZero_anti_attempts += 1;}
    
    // Randomly choose where to insert worm end on the flat interval
    tau_new = tau_flat*rnum(rng);
    
    // Determine the no. of particles after each worm end
    if (is_worm){
        n_tail = n + 1;
        n_head = n;
    }
    else{
        n_tail = n;
        n_head = n - 1;
    }
    
    // Calculate the diagonal energy difference dV = \epsilon_w - \epsilon
    dV = (U/2.0)*(n_tail*(n_tail-1)-n_head*(n_head-1)) - mu*(n_tail-n_head);
    
    // deleteZero (reverse update) might've had to choose either head or tail
    if (head_idx==-1 and tail_idx==-1) // only one end after insertZero
        p_wormend = 1.0;
    else{                              // two worm ends after insertZero
        if (is_worm){
            if (paths[paths[tail_idx].prev].tau != 0)
                p_wormend = 1.0; //cannot choose tail.was not coming from tau=0.
            else
                p_wormend = 0.5; // deleteZero could choose either head or tail
        }
        else{ // if insert anti (i.e, a tail) the end present was a head
            if (paths[paths[head_idx].prev].tau != 0)
                p_wormend = 1.0;
            else
                p_wormend = 0.5;
        }
    }
    
    // Determine the length of the path to be modified
    l_path = tau_new;
    
    // Determine the total particle change based on worm type
    if (is_worm){
        dN = +1.0 * l_path/beta;
    }
    else{
        dN = -1.0 * l_path/beta;
    }
    
    // Canonical simulations: Restrict updates to interval N:(N-1,N+1)
    if (canonical)
        if ((N_tracker+dN) < (N-1) || (N_tracker+dN) > (N+1)){return;}
    
    // Count the TOTAL number of particles at tau=0
    N_b = N_zero;
    
    // Build the weight ratio W'/W
     C = 1.0;
    if (is_worm){
//        C = sqrt(N_b+1)/sqrt(n+1);
        W = eta * sqrt(n_tail) * C * exp(-dV*tau_new);
    }
    else{
//        C = sqrt(n)/sqrt(N_b);
        W = eta * sqrt(n_tail) * C * exp(dV*tau_new);
    }
    
    // Build the Metropolis Ratio (R)
    p_dz = 0.5;
    p_iz = 0.5;
    R = W * (p_dz/p_iz) * M * p_wormend * tau_flat / p_type;

    // Metropolis sampling
    if (rnum(rng) < R){ // Accept
        
        // Activate the first available kink
        if (is_worm){
            paths[num_kinks] = Kink(tau_new,n_head,src,src,src,next,
                                    src_replica,dest_replica);
            
            // Save head index
            head_idx = num_kinks;
            
            // Update the number of particles in the initial kink at site i
            paths[i].n = n_tail;
            
            // Add to Acceptance counter
            insertZero_worm_accepts += 1;
            
            // Worm inserted, add one to tau=0 particle tracker
            N_zero += 1;
        }
        else{ // antiworm
            paths[num_kinks] = Kink(tau_new,n_tail,src,src,src,next,
                                    src_replica,dest_replica);
            
            // Save head index
            tail_idx = num_kinks;
            
            // Update number of particles in initial kink of insertion site
            paths[i].n = n_head;
            
            // Add to Acceptance counter
            insertZero_anti_accepts += 1;
            
            // Antiworm inserted, subtract one to tau=0 particle tracker
            N_zero -= 1;
        }
        
        // "Connect" next of lower bound kink to the new worm end
        paths[src].next = num_kinks;
        
        // "Connect" prev of next kink to the new worm end
        if (next!=-1){paths[next].prev = num_kinks;}
        
        // Update trackers for: no. of active kinks, total particles
        num_kinks += 1;
        N_tracker += dN;
        
        // If new worm end is last kink on site, update last_kinks vector
        if (next==-1){
            if (is_worm){last_kinks[src]=head_idx;}
            else {last_kinks[src]=tail_idx;}
        }
        
        return;
    }
    else // Reject
        return;
}

/*----------------------------------------------------------------------------*/

void deleteZero(vector<Kink> &paths, int &num_kinks, int &head_idx,
                int &tail_idx, int M, int N, double U, double mu, double t,
                double beta, double eta, bool canonical, double &N_tracker,
                int &N_zero, int &N_beta, vector<int> &last_kinks,
                unsigned long long int &deleteZero_worm_attempts,
                unsigned long long int &deleteZero_worm_accepts,
                unsigned long long int &deleteZero_anti_attempts,
                unsigned long long int &deleteZero_anti_accepts,
                boost::random::mt19937 &rng){
    
    // Variable declarations
    int n,src,dest,prev,next,n_head,n_tail,N_b,worm_end_idx;
    double tau,tau_next,tau_flat,l_path,dN,dV,R,p_type,p_wormend,C,W,p_dz,p_iz;
    bool delete_head;

    // Cannot delete if there are no worm ends present
    if (head_idx==-1 && tail_idx==-1){return;}

    // Cannot delete if there are no worm ends coming from tau=0
    if (head_idx!=-1 && tail_idx!=-1){
        if (paths[paths[head_idx].prev].tau != 0 and
            paths[paths[tail_idx].prev].tau != 0){return;}
    }
    else if (head_idx!=-1){ // only head present
        if (paths[paths[head_idx].prev].tau != 0){return;}
    }
    else{ // only tail present
        if (paths[paths[tail_idx].prev].tau != 0){return;}
    }

    // Decide which worm end to delete
    boost::random::uniform_real_distribution<double> rnum(0.0, 1.0);
    if (head_idx!=-1 && tail_idx!=-1){ // both wormends present
        if (paths[paths[head_idx].prev].tau == 0 &&
            paths[paths[tail_idx].prev].tau == 0){ //both near 0
            if (rnum(rng) < 0.5)
                delete_head = true;
            else
                delete_head = false;
            p_wormend = 0.5;
        }
        else if (paths[paths[head_idx].prev].tau == 0){
            delete_head = true;
            p_wormend = 1.0;
        }
        else{ // only the tail is near zero
            delete_head = false;
            p_wormend = 1.0;
        }
    }
    else if (head_idx!=-1){ // only head present
        delete_head = true;
        p_wormend = 1.0;
    }
    else{ // only tail present
        delete_head = false;
        p_wormend = 1.0;
    }

    // Get index of worm end to be deleted
    if (delete_head)
        worm_end_idx = head_idx;
    else
        worm_end_idx = tail_idx;

    // Extract worm end attributes
    tau = paths[worm_end_idx].tau;
    n = paths[worm_end_idx].n;
    src = paths[worm_end_idx].src;
    dest = paths[worm_end_idx].dest;
    prev = paths[worm_end_idx].prev;
    next = paths[worm_end_idx].next;

    // Calculate the length of the flat interval (excluding the wormend)
    if (next == -1)
        tau_next = beta;
    else
        tau_next = paths[next].tau;
    tau_flat = tau_next;

    // No. of particles before,after the worm end to be deleted
    if (delete_head) // delete worm
        n_tail = n+1;
    else //  delete antiworm
        n_tail = n;
    n_head = n_tail-1;
    
    // Worm insert (reverse update) probability of choosing worm or antiworm
    if (head_idx!=-1 and tail_idx!=-1) // worm end present before insertion
        p_type = 1.0;
    else{ // no worm ends present before insertion
        if (n==0)
            p_type = 1.0; // only worm can be inserted if no particles on flat
        else
            p_type = 0.5;
    }

    // Add to deleteZero PROPOSAL counters
    if (delete_head) // delete head (delete worm)
        deleteZero_worm_attempts += 1;
    else                   // delete tail (delete antiworm)
        deleteZero_anti_attempts += 1;
    
    // Determine the length of path to be modified
    l_path = tau;
    
    // Determine the total particle change based on worm type
    if (delete_head) // delete worm
        dN = -1.0 * l_path / beta;
    else // delete anti
        dN = +1.0 * l_path / beta;
    
    // Canonical simulations: Restrict updates to interval N:(N-1,N+1)
    if (canonical)
        if ((N_tracker+dN) < (N-1) || (N_tracker+dN) > (N+1)){return;}
        
    // Calculate diagonal energy difference
    dV = (U/2.0)*(n_tail*(n_tail-1)-n_head*(n_head-1)) - mu*(n_tail-n_head);
    
    // Determine the number of total bosons before the worm/anti was inserted
    if (delete_head)   // delete worm
        N_b = N_zero-1;
    else               // delete antiworm
        N_b = N_zero+1;
    
    // Build the weigh ratio W'/W
     C = 1.0;
    if (delete_head){ // delete worm
//        C = sqrt(N_b+1)/sqrt(n+1);
        W = eta * sqrt(n_tail) * C * exp(-dV*tau);
    }
    else{ // delete antiworm
//        C = sqrt(n)/sqrt(N_b);
        W = eta * sqrt(n_tail) * C * exp(dV*tau);
    }
    
    // Build the Metropolis Ratio  (R)
    p_dz = 0.5;
    p_iz = 0.5;
    R = W * (p_dz/p_iz) * M * p_wormend * tau_flat / p_type;
    R = 1.0/R;
    
    // Metropolis sampling
    if (rnum(rng) < R){ // accept
        
        // Update the number of particles in the initial kink of worm end site
        paths[paths[worm_end_idx].prev].n = n;
        
        // num_kinks-1 (last available kink) will be swapped. Modify links to it
        if (paths[num_kinks-1].next!=-1) // avoids access with -1 index
            paths[paths[num_kinks-1].next].prev = worm_end_idx;
        paths[paths[num_kinks-1].prev].next = worm_end_idx;
        
        swap(paths[worm_end_idx],paths[num_kinks-1]);
        
        // Upper bound of flat could've been swapped. Correct if so.
        if (next==num_kinks-1){next=worm_end_idx;}
                
        // The other worm end could've been swapped. Reindex it if so.
        if (delete_head){
            if (tail_idx==num_kinks-1){tail_idx=worm_end_idx;}
        }
        else{
            if (head_idx==num_kinks-1){head_idx=worm_end_idx;}
        }
        
        // Whatever kink was swapped could've been the last on its site.
        if (paths[worm_end_idx].next==-1){
            last_kinks[paths[worm_end_idx].src]=worm_end_idx;
        }
        
        // If deleted worm end was last kink on site, update last kinks tracker
        if (next==-1){last_kinks[src]=prev;}
        
        // Reconnect the lower,upper bounds of the flat interval.
        paths[prev].next = next;
        if (next!=-1)
            paths[next].prev = prev;

        // Deactivate the worm end
        if (delete_head){
            head_idx = -1;
            
            // Add to Acceptance counter
            deleteZero_worm_accepts += 1;
            
            // Worm deleted, subtract one to tau=0 particle tracker
            N_zero -= 1;
        }
        else{
            tail_idx = -1;
            
            // Add to Acceptance counter
            deleteZero_anti_accepts += 1;
            
            // Antiworm deleted, add one to tau=0 particle tracker
            N_zero += 1;
        }
        
        // Update trackers for: num of active kinks, total particles
        num_kinks -= 1;
        N_tracker += dN;
        
        return;
    }
    else // reject
        return;
}
/*----------------------------------------------------------------------------*/

void insertBeta(vector<Kink> &paths, int &num_kinks, int &head_idx,
                int &tail_idx, int M, int N, double U, double mu, double t,
                double beta, double eta, bool canonical, double &N_tracker,
                int &N_zero, int &N_beta, vector<int> &last_kinks,
                unsigned long long int &insertBeta_worm_attempts,
                unsigned long long int &insertBeta_worm_accepts,
                unsigned long long int &insertBeta_anti_attempts,
                unsigned long long int &insertBeta_anti_accepts,
                boost::random::mt19937 &rng){
    
    // Variable declarations
    int n,src,dest,prev,next,n_head,n_tail,i,N_b,src_replica,dest_replica;
    double tau_prev,tau_flat,
    l_path,dN,dV,R,p_type,tau_new,p_wormend,C,W,p_db,p_ib;
    bool is_worm;

    // Cannot insert if there's two worm ends present
    if (head_idx != -1 and tail_idx != -1){return;}
        
    // Randomly select site on which to insert worm/antiworm from tau=0
    boost::random::uniform_int_distribution<> sites(0, M-1);
    i = sites(rng);
    
    // Extract the flat interval where insertion is proposed & its attributes
    tau_prev = paths[last_kinks[i]].tau;
    n = paths[last_kinks[i]].n;
    src = paths[last_kinks[i]].src;
    dest = paths[last_kinks[i]].dest;
    prev = paths[last_kinks[i]].prev;
    next = paths[last_kinks[i]].next;
    src_replica = paths[last_kinks[i]].src_replica;
    dest_replica = paths[last_kinks[i]].dest_replica;
    
    // Determine the length of insertion flat interval
    tau_flat = beta - tau_prev;
    
    // Choose worm/antiworm insertion based on worm ends present
    boost::random::uniform_real_distribution<double> rnum(0.0, 1.0);
    if (head_idx==-1 and tail_idx==-1){ // no worm ends present
        if (n==0){ // can only insert worm, not antiworm
            is_worm = true;
            p_type = 1.0;
        }
        else{ // choose worm or antiworm insertion with equal probability
            if (rnum(rng) < 0.5)
                is_worm = true;
            else
                is_worm = false;
            p_type = 0.5;
        }
    }
    else if (tail_idx!=-1){ // only worm tail present, can insert antiworm only
        if (n==0){
            insertBeta_anti_attempts += 1.0;
            return; // cannot insert proposed antiworm, no particles present
        }
        else{
            is_worm = false;
            p_type = 1.0;
        }
    }
    else{ // only head present, can insert worm only
        is_worm = true;
        p_type = 1.0;
    }
    
    // Add to worm/antiworm insertion attempt counters
    if (is_worm){insertBeta_worm_attempts += 1;}
    else {insertBeta_anti_attempts += 1;}
    
    // Randomly choose where to insert worm end on the flat interval
    tau_new = tau_prev + tau_flat*rnum(rng);
    
    // Determine the no. of particles after each worm end
    if (is_worm){
        n_tail = n + 1;
        n_head = n;
    }
    else{
        n_tail = n;
        n_head = n - 1;
    }
    
    // Calculate the diagonal energy difference dV = \epsilon_w - \epsilon
    dV = (U/2.0)*(n_tail*(n_tail-1)-n_head*(n_head-1)) - mu*(n_tail-n_head);
    
    // deleteBeta (reverse update) might've had to choose either head or tail
    if (head_idx==-1 and tail_idx==-1) // only one end after insertZero
        p_wormend = 1.0;
    else{                              // two worm ends after insertZero
        if (is_worm){
            if (paths[head_idx].next != -1)
                p_wormend = 1.0; // cannot choose head.was not coming from beta.
            else
                p_wormend = 0.5; // deleteBeta could choose either head or tail
        }
        else{ // if insert anti (i.e, a head) the end present was a tail
            if (paths[tail_idx].next != -1)
                p_wormend = 1.0;
            else
                p_wormend = 0.5;
        }
    }
    
    // Determine the length of the path to be modified
    l_path = beta - tau_new;
    
    // Determine the total particle change based on worm type
    if (is_worm){
        dN = +1.0 * l_path/beta;
    }
    else{
        dN = -1.0 * l_path/beta;
    }
    
    // Canonical simulations: Restrict updates to interval N:(N-1,N+1)
    if (canonical)
        if ((N_tracker+dN) < (N-1) || (N_tracker+dN) > (N+1)){return;}
    
    // Count the TOTAL number of particles at tau=0
    N_b = N_beta;
    
    // Build the weight ratio W'/W
     C = 1.0; // C_pre/C_post
    if (is_worm){
//        C = sqrt(N_b+1)/sqrt(n+1);
        W = eta * sqrt(n_tail) * C * exp(-dV*(beta-tau_new));
    }
    else{
//        C = sqrt(n)/sqrt(N_b);
        W = eta * sqrt(n_tail) * C * exp(-dV*(tau_new-beta));
    }
    
    // Build the Metropolis Ratio (R)
    p_db = 0.5;
    p_ib = 0.5;
    R = W * (p_db/p_ib) * M * p_wormend * tau_flat / p_type;

    // Metropolis sampling
    if (rnum(rng) < R){ // Accept
        
        // Activate the first available kink
        if (is_worm){
            paths[num_kinks] = Kink (tau_new,n_tail,src,src,
                                            last_kinks[src],next,
                                            src_replica,dest_replica);
            
            // Save tail index
            tail_idx = num_kinks;
            
            // Add to Acceptance counter
            insertBeta_worm_accepts += 1;
            
            // Worm inserted, add one to tau=beta particle tracker
            N_beta += 1;
        }
        else{ // antiworm
            paths[num_kinks] = Kink (tau_new,n_head,src,src,
                                            last_kinks[src],next,
                                            src_replica,dest_replica);
            
            // Save head index
            head_idx = num_kinks;
            
            // Add to Acceptance counter
            insertBeta_anti_accepts += 1;
            
            // Antiworm inserted, subtract one to tau=beta particle tracker
            N_beta -= 1;
        }
        
        // "Connect" next of lower bound kink to the new worm end
        paths[last_kinks[src]].next = num_kinks;
        
        // Update trackers for: no. of active kinks, total particles
        num_kinks += 1;
        N_tracker += dN;
        
        // If new worm end is last kink on site, update last_kinks vector
        if (next==-1){
            if (is_worm){last_kinks[src]=tail_idx;}
            else {last_kinks[src]=head_idx;}
        }
        
        return;
    }
    else // Reject
        return;
}

/*----------------------------------------------------------------------------*/

void deleteBeta(vector<Kink> &paths, int &num_kinks, int &head_idx,
                int &tail_idx, int M, int N, double U, double mu, double t,
                double beta, double eta, bool canonical, double &N_tracker,
                int &N_zero, int &N_beta, vector<int> &last_kinks,
                unsigned long long int &deleteBeta_worm_attempts,
                unsigned long long int &deleteBeta_worm_accepts,
                unsigned long long int &deleteBeta_anti_attempts,
                unsigned long long int &deleteBeta_anti_accepts,
                boost::random::mt19937 &rng){
    
    // Variable declarations
    int n,src,dest,prev,next,n_head,n_tail,N_b,worm_end_idx;
    double tau,tau_prev,tau_flat,l_path,dN,dV,R,p_type,p_wormend,C,W,p_db,p_ib;
    bool delete_head;

    // Cannot delete if there are no worm ends present
    if (head_idx==-1 && tail_idx==-1){return;}

    // Cannot delete if there are no worm ends coming from tau=beta
    if (head_idx!=-1 && tail_idx!=-1){
        if (paths[head_idx].next != -1 and
            paths[tail_idx].next != -1){return;}
    }
    else if (head_idx!=-1){ // only head present
        if (paths[head_idx].next != -1){return;}
    }
    else{ // only tail present
        if (paths[tail_idx].next != -1){return;}
    }
    
    // Decide which worm end to delete
    boost::random::uniform_real_distribution<double> rnum(0.0, 1.0);
    if (head_idx!=-1 && tail_idx!=-1){ // both wormends present
        if (paths[head_idx].next == -1 &&
            paths[tail_idx].next == -1){ // both last
            if (rnum(rng) < 0.5)
                delete_head = true;  // delete antiworm
            else
                delete_head = false; // delete worm
            p_wormend = 0.5;
        }
        else if (paths[head_idx].next == -1){
            delete_head = true;
            p_wormend = 1.0;
        }
        else{ // only the tail is near zero
            delete_head = false;
            p_wormend = 1.0;
        }
    }
    else if (head_idx!=-1){ // only head present
        delete_head = true;
        p_wormend = 1.0;
    }
    else{ // only tail present
        delete_head = false;
        p_wormend = 1.0;
    }

    // Get index of worm end to be deleted
    if (delete_head)
        worm_end_idx = head_idx;
    else
        worm_end_idx = tail_idx;

    // Extract worm end attributes
    tau = paths[worm_end_idx].tau;
    n = paths[worm_end_idx].n;
    src = paths[worm_end_idx].src;
    dest = paths[worm_end_idx].dest;
    prev = paths[worm_end_idx].prev;
    next = paths[worm_end_idx].next;

    // Calculate the length of the flat interval (excluding the worm end)
    tau_prev = paths[prev].tau;
    tau_flat = beta - tau_prev;

    // No. of particles before,after the worm end to be deleted
    if (delete_head) // delete antiworm
        n_tail = n+1;
    else //  delete worm
        n_tail = n;
    n_head = n_tail-1;
    
    // Worm insert (reverse update) probability of choosing worm or antiworm
    if (head_idx!=-1 and tail_idx!=-1) // worm end present before insertion
        p_type = 1.0;
    else{ // no worm ends present before insertion
        if (paths[paths[worm_end_idx].prev].n==0)
            p_type = 1.0; // only worm can be inserted if no particles on flat
        else
            p_type = 0.5;
    }

    // Add to deleteBeta PROPOSAL counters
    if (delete_head) // delete antiworm
        deleteBeta_anti_attempts += 1;
    else                   // delete worm
        deleteBeta_worm_attempts += 1;
    
    // Determine the length of path to be modified
    l_path = beta - tau;
    
    // Determine the total particle change based on worm type
    if (delete_head) // delete antiworm
        dN = +1.0 * l_path / beta;
    else // delete worm
        dN = -1.0 * l_path / beta;
    
    // Canonical simulations: Restrict updates to interval N:(N-1,N+1)
    if (canonical)
        if ((N_tracker+dN) < (N-1) || (N_tracker+dN) > (N+1)){return;}
    
    // Calculate diagonal energy difference
    dV = (U/2.0)*(n_tail*(n_tail-1)-n_head*(n_head-1)) - mu*(n_tail-n_head);
    
    // Determine the number of total bosons before the worm/anti was inserted
    if (delete_head)     // delete antiworm
        N_b = N_beta+1;
    else                 // delete worm
        N_b = N_beta-1;
    
    // Build the weigh ratio W'/W
    C = 1.0;
    if (!delete_head){ // delete worm
//        C = sqrt(N_b+1)/sqrt(n);
        W = eta * sqrt(n_tail) * C * exp(-dV*(beta-tau));
    }
    else{ // delete antiworm
//        C = sqrt(n+1)/sqrt(N_b);
        W = eta * sqrt(n_tail) * C * exp(-dV*(tau-beta));
    }
    
    // Build the Metropolis Ratio  (R)
    p_db = 0.5;
    p_ib = 0.5;
    R = W * (p_db/p_ib) * M * p_wormend * tau_flat / p_type;
    R = 1.0/R;
    
    // Metropolis sampling
    if (rnum(rng) < R){ // accept
                
        // num_kinks-1 (last available kink) will be swapped. Modify links to it
        if (paths[num_kinks-1].next!=-1) // avoids access with -1 index
            paths[paths[num_kinks-1].next].prev = worm_end_idx;
        paths[paths[num_kinks-1].prev].next = worm_end_idx;
        
        swap(paths[worm_end_idx],paths[num_kinks-1]);
        
        // Lower bound of flat could've been swapped. Correct if so.
        if (prev==num_kinks-1){prev=worm_end_idx;}
        
        // The other worm end could've been swapped. Reindex it if so.
        if (delete_head){
            if (tail_idx==num_kinks-1){tail_idx=worm_end_idx;}
        }
        else{
            if (head_idx==num_kinks-1){head_idx=worm_end_idx;}
        }
        
        // Whatever kink was swapped could've been the last on its site.
        if (paths[worm_end_idx].next==-1){
            last_kinks[paths[worm_end_idx].src]=worm_end_idx;
        }
        
        // Deleted worm end was last kink on site, update last kinks tracker
        last_kinks[src]=prev;
        
        // Reconnect the lower,upper bounds of the flat interval.
        paths[prev].next = next;

        // Deactivate the worm end
        if (delete_head){
            head_idx = -1;
            
            // Add to Acceptance counter
            deleteBeta_anti_accepts += 1;
            
            // Antiworm deleted, add one to tau=beta particle tracker
            N_beta += 1;
        }
        else{
            tail_idx = -1;
            
            // Add to Acceptance counter
            deleteBeta_worm_accepts += 1;
            
            // Worm deleted, subtracts one to tau=beta particle tracker
            N_beta -= 1;
        }
        
        // Update trackers for: num of active kinks, total particles
        num_kinks -= 1;
        N_tracker += dN;
        
        return;
    }
    else // reject
        return;
}
/*----------------------------------------------------------------------------*/

void timeshift_uniform(vector<Kink> &paths, int &num_kinks,int &head_idx,
                int &tail_idx, int M, int N, double U, double mu, double t,
                double beta, double eta, bool canonical, double &N_tracker,
                int &N_zero, int &N_beta, vector<int> &last_kinks,
                unsigned long long int &advance_head_attempts,
                unsigned long long int &advance_head_accepts,
                unsigned long long int &recede_head_attempts,
                unsigned long long int &recede_head_accepts,
                unsigned long long int &advance_tail_attempts,
                unsigned long long int &advance_tail_accepts,
                unsigned long long int &recede_tail_attempts,
                unsigned long long int &recede_tail_accepts,
                boost::random::mt19937 &rng){
    
    // Variable declarations
    int n,src,dest,prev,next,worm_end_idx;
    double tau,tau_h,tau_t,tau_prev,tau_next,tau_flat,l_path,dN,dV,R,tau_new,W;
    bool shift_head;
    
    // Reject update if there are is no worm end present
    if (head_idx==-1 && tail_idx==-1){return;}

    // Choose which worm end to move
    boost::random::uniform_real_distribution<double> rnum(0.0, 1.0);
    if (head_idx!=-1 && tail_idx!=-1){ // both worm ends present
        tau_h = paths[head_idx].tau;
        tau_t = paths[tail_idx].tau;

        // Randomly choose to shift HEAD or TAIL
        if (rnum(rng) < 0.5)
            shift_head = true;
        else
            shift_head = false;
        }
    else if (head_idx!=-1){ // only head present
        tau_h = paths[head_idx].tau;
        shift_head = true;
    }
    else{ // only tail present
        tau_t = paths[tail_idx].tau;
        shift_head = false;
    }
    
    // Save the kink index of the end that will be shifted
    if (shift_head){worm_end_idx=head_idx;}
    else {worm_end_idx=tail_idx;}
    
    // Extract worm end attributes
    tau = paths[worm_end_idx].tau;
    n = paths[worm_end_idx].n;
    src = paths[worm_end_idx].src;
    dest = paths[worm_end_idx].dest;
    prev = paths[worm_end_idx].prev;
    next = paths[worm_end_idx].next;
    
    // Measure diagonal energy difference dV
    if (shift_head)
        dV=U*n-mu;
    else
        dV=U*(n-1)-mu;
    
    // Determine the lower and upper bounds of the worm end to be timeshifted
    if (next==-1)
        tau_next = beta;
    else
        tau_next = paths[next].tau;
    tau_prev = paths[prev].tau;
    
    // Calculate length of flat interval
    tau_flat = tau_next - tau_prev;

    // Sample the new time of the worm end from uniform distribution
    tau_new = tau_prev + tau_flat*rnum(rng);
    
    // Add to PROPOSAL counter
    if (shift_head){
        if (tau_new > tau){advance_head_attempts+=1;}
        else{recede_head_attempts+=1;}
    }
    else{ // shift tail
        if (tau_new > tau){advance_tail_attempts+=1;}
        else{recede_tail_attempts+=1;}
    }
    
    // Determine the length of path to be modified
    l_path = tau_new - tau;
    
    // Determine the total particle change based on wormend to be shifted
    if (shift_head)
        dN = +1.0 * l_path/beta;
    else // shift tail
        dN = -1.0 * l_path/beta;
    
    // Canonical simulations: Restrict updates to interval N:(N-1,N+1)
    if (canonical)
        if ((N_tracker+dN) < (N-1) || (N_tracker+dN) > (N+1)){return;}
    
    // Build the Metropolis condition (R)
    if (shift_head){
        W = exp(-dV*(tau_new-tau));
    }
    else{
        W = exp(-dV*(tau-tau_new));
    }
    R = W; // recall that timeshift is it's own inverse

    // Metropolis sampling
    if (rnum(rng) < R){
        
        // Add to ACCEPTANCE counter
        if (shift_head){
            if (tau_new > tau){advance_head_accepts+=1;}
            else{recede_head_accepts+=1;}
        }
        else{ // shift tail
            if (tau_new > tau){advance_tail_accepts+=1;}
            else{recede_tail_accepts+=1;}
        }
        
        // Modify the worm end time
        paths[worm_end_idx].tau = tau_new;
        
        // Modify total particle number tracker
        N_tracker += dN;
        
        return;
    }
    else // Reject
        return;
}

/*----------------------------------------------------------------------------*/

void timeshift(vector<Kink> &paths, int &num_kinks, int &head_idx,
                int &tail_idx, int M, int N, double U, double mu, double t,
                double beta, double eta, bool canonical, double &N_tracker,
                int &N_zero, int &N_beta, vector<int> &last_kinks,
                unsigned long long int &advance_head_attempts,
                unsigned long long int &advance_head_accepts,
                unsigned long long int &recede_head_attempts,
                unsigned long long int &recede_head_accepts,
                unsigned long long int &advance_tail_attempts,
                unsigned long long int &advance_tail_accepts,
                unsigned long long int &recede_tail_attempts,
                unsigned long long int &recede_tail_accepts,
                boost::random::mt19937 &rng){
    
    // Variable declarations
    int n,src,dest,prev,next,worm_end_idx;
    double tau,tau_h,tau_t,tau_prev,tau_next,tau_flat,l_path,dN,dV,R,tau_new,Z;
    bool shift_head;
    
    // Reject update if there is no worm end present
    if (head_idx==-1 && tail_idx==-1){return;}

    // Choose which worm end to move
    boost::random::uniform_real_distribution<double> rnum(0.0, 1.0);
    if (head_idx!=-1 && tail_idx!=-1){ // both worm ends present
        tau_h = paths[head_idx].tau;
        tau_t = paths[tail_idx].tau;

        // Randomly choose to shift HEAD or TAIL
        if (rnum(rng) < 0.5)
            shift_head = true;
        else
            shift_head = false;
        }
    else if (head_idx!=-1){ // only head present
        tau_h = paths[head_idx].tau;
        shift_head = true;
    }
    else{ // only tail present
        tau_t = paths[tail_idx].tau;
        shift_head = false;
    }
    
    // Save the kink index of the end that will be shifted
    if (shift_head){worm_end_idx=head_idx;}
    else {worm_end_idx=tail_idx;}
    
    // Extract worm end attributes
    tau = paths[worm_end_idx].tau;
    n = paths[worm_end_idx].n;
    src = paths[worm_end_idx].src;
    dest = paths[worm_end_idx].dest;
    prev = paths[worm_end_idx].prev;
    next = paths[worm_end_idx].next;
    
    dV=U*(n-!shift_head)-mu;
    
    // To make acceptance ratio unity,shift tail needs to sample w/ dV=eps-eps_w
    if (!shift_head){dV *= -1;} // dV=eps-eps_w
        
    // Determine the lower and upper bounds of the worm end to be timeshifted
    if (next==-1)
        tau_next = beta;
    else
        tau_next = paths[next].tau;
    tau_prev = paths[prev].tau;
    
    // Calculate length of flat interval
    tau_flat = tau_next - tau_prev;

    // Sample the new time of the worm end from truncated exponential dist.
    /*:::::::::::::::::::: Truncated Exponential RVS :::::::::::::::::::::::::*/
    Z = 1.0 - exp(-dV*(tau_next-tau_prev));
    tau_new = tau_prev - log(1.0-Z*rnum(rng))  / dV;
    /*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
    
    // Add to PROPOSAL counter
    if (shift_head){
        if (tau_new > tau){advance_head_attempts+=1;}
        else{recede_head_attempts+=1;}
    }
    else{ // shift tail
        if (tau_new > tau){advance_tail_attempts+=1;}
        else{recede_tail_attempts+=1;}
    }
    
    // Determine the length of path to be modified
    l_path = tau_new - tau;
    
    // Determine the total particle change based on wormend to be shifted
    if (shift_head)
        dN = +1.0 * l_path/beta;
    else // shift tail
        dN = -1.0 * l_path/beta;
    
    // Canonical simulations: Restrict updates to interval N:(N-1,N+1)
    if (canonical)
        if ((N_tracker+dN) < (N-1) || (N_tracker+dN) > (N+1)){return;}
    
    // Build the Metropolis condition (R)
    R = 1.0; // Sampling worm end time from truncated exponential makes R unity.

    // Metropolis sampling
    if (rnum(rng) < R){
        
        // Add to ACCEPTANCE counter
        if (shift_head){
            if (tau_new > tau){advance_head_accepts+=1;}
            else{recede_head_accepts+=1;}
        }
        else{ // shift tail
            if (tau_new > tau){advance_tail_accepts+=1;}
            else{recede_tail_accepts+=1;}
        }
        
        // Modify the worm end time
        paths[worm_end_idx].tau = tau_new;
        
        // Modify total particle number tracker
        N_tracker += dN;
        
        return;
    }
    else // Reject
        return;
}

/*----------------------------------------------------------------------------*/

void insert_kink_before_head(vector<Kink> &paths, int &num_kinks,
                int &head_idx,int &tail_idx,
                int M, int N, double U, double mu, double t,
                vector<vector<int>> &adjacency_matrix, int total_nn,
                double beta, double eta, bool canonical, double &N_tracker,
                int &N_zero, int &N_beta, vector<int> &last_kinks,
                unsigned long long int &ikbh_attempts,
                unsigned long long int &ikbh_accepts,
                boost::random::mt19937 &rng){
    
    // Variable declarations
    int prev,i,j,n_i,n_wi,n_j,n_wj,prev_i,prev_j,next_i,next_j,
    src_replica,dest_replica;
    double tau,tau_h,p_site,W,R,p_dkbh,p_ikbh,tau_prev_i,tau_prev_j,
    tau_kink,tau_min,dV_i,dV_j;
        
    // Update only possible if worm head present
    if (head_idx==-1){return;}
    
    // Need at least two sites to perform a spaceshift
    if (M<2){return;}
        
    // Add to proposal counter
    ikbh_attempts += 1;
    
    // Extract the worm head site and replica
    i = paths[head_idx].src;
    src_replica = paths[head_idx].src_replica;
    dest_replica = paths[head_idx].dest_replica;
    
    // Randomly choose a nearest neighbor site
    boost::random::uniform_int_distribution<> random_nn(0, total_nn-1);
    j = adjacency_matrix[i][random_nn(rng)];
    p_site = 1.0/total_nn;
    
    // Retrieve the time of the worm head
    tau_h = paths[head_idx].tau;
    
    // Determine index of lower/upper kinks of flat where head is (site i)
    prev_i = paths[head_idx].prev;
    next_i = paths[head_idx].next;
    
    // Determine index of lower/upper kinks of flat where head jumps to (site j)
    tau = 0.0;            // tau_prev_j candidate
    prev = j;           // prev_j candidate
    prev_j = j;         // this avoids "variable maybe not initialized" warning
    while (tau<tau_h){
        // Set the lower bound index
        prev_j = prev;
        
        // Update lower bound index and tau candidates for next iteration
        prev = paths[prev].next;
        if (prev==-1){break;}
        tau = paths[prev].tau;
    }
    next_j=prev;
    
    // Determine upper,lower bound times on both sites (upper time not needed)
    tau_prev_i = paths[prev_i].tau;
    tau_prev_j = paths[prev_j].tau;
    
    // Determine lowest time at which kink could've been inserted
    if (tau_prev_i>tau_prev_j){tau_min=tau_prev_i;}
    else {tau_min=tau_prev_j;}
    
    // Randomly choose the time of the kink
    boost::random::uniform_real_distribution<double> rnum(0.0, 1.0);
    tau_kink = tau_min + rnum(rng)*(tau_h-tau_min);
    if (tau_kink == tau_min){return;}
    
    // Extract no. of particles in the flats adjacent to the new kink
    n_wi = paths[prev_i].n;
    n_i = n_wi-1;
    n_j = paths[prev_j].n;
    n_wj = n_j+1;                   // "w": segment with the extra particle
        
    // Calculate the diagonal energy difference on both sites
    dV_i = (U/2.0)*(n_wi*(n_wi-1)-n_i*(n_i-1)) - mu*(n_wi-n_i);
    dV_j = (U/2.0)*(n_wj*(n_wj-1)-n_j*(n_j-1)) - mu*(n_wj-n_j);
    
    // Calculate the weight ratio W'/W
    W = t * n_wj * exp((dV_i-dV_j)*(tau_h-tau_kink));
    
    // Build the Metropolis ratio (R)
    p_dkbh = 0.5;
    p_ikbh = 0.5;
    R = W * (p_dkbh/p_ikbh) * (tau_h-tau_min)/p_site;
    
    // Metropolis Sampling
    if (rnum(rng) < R){ // Accept
        
        // Add to acceptance counter
        ikbh_accepts += 1;
                
        // Change kink that stored head information to a regular kink
        paths[head_idx].tau = tau_kink;
        paths[head_idx].n = n_i;
        paths[head_idx].src = i;  // more like site
        paths[head_idx].dest = j; // more like connecting site
        paths[head_idx].prev = prev_i;
        paths[head_idx].next = next_i;
        
        // Create the kinks on the destination site
        paths[num_kinks]=Kink(tau_kink,n_wj,j,i,prev_j,num_kinks+1,
                              src_replica,dest_replica);
        paths[num_kinks+1]=Kink(tau_h,n_j,j,j,num_kinks,next_j,
                                src_replica,dest_replica);
        
        // Set new worm head index
        head_idx = num_kinks+1;
                
        // "Connect" next of lower bound kink to new kink
        paths[prev_j].next = num_kinks;
        
        // "Connect" prev of next kink to worm head
        if(next_j!=-1){paths[next_j].prev = head_idx;}
                
        // Update number of kinks tracker
        num_kinks += 2;
        
        // If worm head is last kink on site j, update last kinks tracker vector
        if (next_j==-1){last_kinks[j]=head_idx;}
        
        return;
        
    }
    else // Reject
        return;
    }

/*----------------------------------------------------------------------------*/

void delete_kink_before_head(vector<Kink> &paths, int &num_kinks,
                int &head_idx,int &tail_idx,
                int M, int N, double U, double mu, double t,
                vector<vector<int>> &adjacency_matrix, int total_nn,
                double beta, double eta, bool canonical, double &N_tracker,
                int &N_zero, int &N_beta, vector<int> &last_kinks,
                unsigned long long int &dkbh_attempts,
                unsigned long long int &dkbh_accepts,
                boost::random::mt19937 &rng){

    // Variable declarations
    int prev,i,j,n_i,n_wi,n_j,n_wj,prev_i,prev_j,next_i,next_j,
    kink_idx_i,kink_idx_j,src_replica,dest_replica;
    double tau,tau_h,p_site,W,R,p_dkbh,p_ikbh,tau_prev_i,tau_prev_j,
    tau_kink,tau_min,dV_i,dV_j,tau_next_i;

    // Update only possible if worm head present
    if (head_idx==-1){return;}

    // There has to be a regular kink before the worm head
    if (paths[paths[head_idx].prev].src
    ==paths[paths[head_idx].prev].dest){return;}

    // Need at least two sites to perform a spaceshift
    if (M<2){return;}

    // Indices of: upper bound kink, kink before head, lower bound kink ; site j
    next_j = paths[head_idx].next;
    kink_idx_j = paths[head_idx].prev;
    prev_j = paths[kink_idx_j].prev;

    // Times of: worm head, kink before head, lower bound kink; site j
    tau_h = paths[head_idx].tau;
    tau_kink = paths[kink_idx_j].tau;
    tau_prev_j = paths[prev_j].tau;

    // Only kinks in which the particle hops from i TO j can be deleted
    if (paths[kink_idx_j].n-paths[prev_j].n<0){return;}

    // Retrieve worm head site (j) and connecting site (i)
    j = paths[kink_idx_j].src;
    i = paths[kink_idx_j].dest;
    src_replica = paths[kink_idx_j].src_replica;
    dest_replica = paths[kink_idx_j].dest_replica;
    
    // Determine index of lower/upper bounds of flat where kink connects to (i)
    tau = 0.0;            // tau_prev_i candidate
    prev = i;           // prev_i candidate
    prev_i = i;         // this avoids "variable maybe not initialized" warning
    while (tau<tau_kink){
        // Set the lower bound index
        prev_i = prev;

        // Update lower bound index and tau candidates for next iteration
        prev = paths[prev].next;
        if (prev==-1){break;}
        tau = paths[prev].tau;
    }
    kink_idx_i = prev;
    next_i=paths[kink_idx_i].next;

    // Retrieve time of lower,upper bounds on connecting site (i)
    tau_prev_i = paths[prev_i].tau;
    if (next_i!=-1){tau_next_i = paths[next_i].tau;}
    else{tau_next_i=beta;}

    // Deletion cannot interfere w/ kinks on other site
    if (tau_h >= tau_next_i){return;}

    // Add to proposal counter
    dkbh_attempts += 1;

    // Determine lowest time at which kink could've been inserted
    if (tau_prev_i>tau_prev_j){tau_min=tau_prev_i;}
    else {tau_min=tau_prev_j;}

    // Probability of inverse move (ikbh) of choosing site where worm end is
    p_site = 1.0/total_nn;

    // Extract no. of particles in the flats adjacent to the new kink
    n_wi = paths[prev_i].n;
    n_i = n_wi-1;
    n_j = paths[prev_j].n;
    n_wj = n_j+1;                   // "w": segment with the extra particle

    // Calculate the diagonal energy difference on both sites
    dV_i = (U/2.0)*(n_wi*(n_wi-1)-n_i*(n_i-1)) - mu*(n_wi-n_i);
    dV_j = (U/2.0)*(n_wj*(n_wj-1)-n_j*(n_j-1)) - mu*(n_wj-n_j);

    // Calculate the weight ratio W'/W
    W = t * n_wj * exp((dV_i-dV_j)*(tau_h-tau_kink));

    // Build the Metropolis ratio (R)
    p_dkbh = 0.5;
    p_ikbh = 0.5;
    R = W * (p_dkbh/p_ikbh) * (tau_h-tau_min)/p_site;
    R = 1.0/R;

    // Metropolis Sampling
    boost::random::uniform_real_distribution<double> rnum(0.0, 1.0);
    if (rnum(rng) < R){ // Accept

        // Add to acceptance counter
        dkbh_accepts += 1;

        // Stage 1: Delete kink on i
        if (paths[num_kinks-1].next!=-1)
            paths[paths[num_kinks-1].next].prev = kink_idx_i;
        paths[paths[num_kinks-1].prev].next = kink_idx_i;

        swap(paths[kink_idx_i],paths[num_kinks-1]);

        if (prev_i==num_kinks-1){prev_i=kink_idx_i;}
        else if (next_i==num_kinks-1){next_i=kink_idx_i;}
        else if (prev_j==num_kinks-1){prev_j=kink_idx_i;}
        else if (next_j==num_kinks-1){next_j=kink_idx_i;}
        else if (kink_idx_j==num_kinks-1){kink_idx_j=kink_idx_i;}
        else if (head_idx==num_kinks-1){head_idx=kink_idx_i;}
        else {;}

        if (tail_idx==num_kinks-1){tail_idx=kink_idx_i;}

        if (paths[kink_idx_i].next==-1){
            last_kinks[paths[kink_idx_i].src]=kink_idx_i;
        }

        if (next_i!=-1)
            paths[next_i].prev = prev_i;
        paths[prev_i].next = next_i;

        if (next_i==-1){last_kinks[i]=prev_i;}

        // Stage 2: Delete worm head on j
        if (paths[num_kinks-2].next!=-1)
            paths[paths[num_kinks-2].next].prev = head_idx;
        paths[paths[num_kinks-2].prev].next = head_idx;

        swap(paths[head_idx],paths[num_kinks-2]);

        if (prev_i==num_kinks-2){prev_i=head_idx;}
        else if (next_i==num_kinks-2){next_i=head_idx;}
        else if (prev_j==num_kinks-2){prev_j=head_idx;}
        else if (next_j==num_kinks-2){next_j=head_idx;}
        else if (kink_idx_j==num_kinks-2){kink_idx_j=head_idx;}
        else {;}

        if (tail_idx==num_kinks-2){tail_idx=head_idx;}

        if (paths[head_idx].next==-1){
            last_kinks[paths[head_idx].src]=head_idx;
        }

        if (next_j!=-1)
            paths[next_j].prev = kink_idx_j;
        paths[kink_idx_j].next = next_j;

        if (next_j==-1){last_kinks[j]=kink_idx_j;}

        // Stage 3: Delete kink on j
        if (paths[num_kinks-3].next!=-1)
            paths[paths[num_kinks-3].next].prev = kink_idx_j;
        paths[paths[num_kinks-3].prev].next = kink_idx_j;

        swap(paths[kink_idx_j],paths[num_kinks-3]);

        if (prev_i==num_kinks-3){prev_i=kink_idx_j;}
        else if (next_i==num_kinks-3){next_i=kink_idx_j;}
        else if (prev_j==num_kinks-3){prev_j=kink_idx_j;}
        else if (next_j==num_kinks-3){next_j=kink_idx_j;}
        else {;}

        if (tail_idx==num_kinks-3){tail_idx=kink_idx_j;}

        if (paths[kink_idx_j].next==-1){
            last_kinks[paths[kink_idx_j].src]=kink_idx_j;
        }

        if (next_j!=-1)
            paths[next_j].prev = prev_j;
        paths[prev_j].next = next_j;

        if (next_j==-1){last_kinks[j]=prev_j;}

        // Stage 4: Insert worm head on i
        paths[num_kinks-3]=Kink(tau_h,n_i,i,i,prev_i,next_i,
                                src_replica,dest_replica);

        head_idx = num_kinks-3;

        paths[prev_i].next = head_idx;
        if(next_i!=-1){paths[next_i].prev = head_idx;}

        if (next_i==-1){last_kinks[i]=head_idx;}

        // Update number of kinks tracker
        num_kinks -= 2;

        return;

    }
    else // Reject
        return;
    }

/*----------------------------------------------------------------------------*/

void insert_kink_after_head(vector<Kink> &paths, int &num_kinks,
                int &head_idx,int &tail_idx,
                int M, int N, double U, double mu, double t,
                vector<vector<int>> &adjacency_matrix, int total_nn,
                double beta, double eta, bool canonical, double &N_tracker,
                int &N_zero, int &N_beta, vector<int> &last_kinks,
                unsigned long long int &ikah_attempts,
                unsigned long long int &ikah_accepts,
                boost::random::mt19937 &rng){
    
    // Variable declarations
    int prev,i,j,n_i,n_wi,n_j,n_wj,prev_i,prev_j,next_i,next_j,
    src_replica,dest_replica;
    double tau,tau_h,p_site,W,R,p_dkah,p_ikah,tau_prev_i,tau_prev_j,
    tau_kink,tau_max,dV_i,dV_j,tau_next_i,tau_next_j;
    
    // Update only possible if worm head present
    if (head_idx==-1){return;}
    
    // Need at least two sites to perform a spaceshift
    if (M<2){return;}
    
    // Add to proposal counter
    ikah_attempts += 1;
    
    // Extract the worm head site
    i = paths[head_idx].src;
    src_replica = paths[head_idx].src_replica;
    dest_replica = paths[head_idx].dest_replica;

    // Randomly choose a nearest neighbor site
    boost::random::uniform_int_distribution<> random_nn(0, total_nn-1);
    j = adjacency_matrix[i][random_nn(rng)];
    p_site = 1.0/total_nn;
    
    // Retrieve the time of the worm head
    tau_h = paths[head_idx].tau;
    
    // Determine index of lower/upper kinks of flat where head is (site i)
    prev_i = paths[head_idx].prev;
    next_i = paths[head_idx].next;
    
    // Determine index of lower/upper kinks of flat where head jumps to (site j)
    tau = 0.0;            // tau_prev_j candidate
    prev = j;           // prev_j candidate
    prev_j = j;         // this avoids "variable maybe not initialized" warning
    while (tau<tau_h){

        // Set the lower bound index
        prev_j = prev;
        
        // Update lower bound index and tau candidates for next iteration
        prev = paths[prev].next;
        if (prev==-1){break;}
        tau = paths[prev].tau;
    }
    next_j=prev;
    
    // Determine upper,lower bound times on both sites
    tau_prev_i = paths[prev_i].tau;
    tau_prev_j = paths[prev_j].tau;
    if (next_i!=-1)
        tau_next_i = paths[next_i].tau;
    else
        tau_next_i = beta;
    if (next_j!=-1)
        tau_next_j = paths[next_j].tau;
    else
        tau_next_j = beta;
    
    // Determine highest time at which kink could've been inserted
    if (tau_next_i<tau_next_j){tau_max=tau_next_i;}
    else {tau_max=tau_next_j;}
    
    // Randomly choose the time of the kink
    boost::random::uniform_real_distribution<double> rnum(0.0, 1.0);
    tau_kink = tau_h + rnum(rng)*(tau_max-tau_h);
    if (tau_kink==tau_h){return;}
    
     // Extract no. of particles in the flats adjacent to the new kink
     n_wi = paths[prev_i].n;
     n_i = n_wi-1;
     n_wj = paths[prev_j].n;
     n_j = n_wj-1;                   // "w": segment with the extra particle
    
    // Update not possible if no particles on destinaton site (j)
    if (n_wj==0){return;}

    // Calculate the diagonal energy difference on both sites
    dV_i = (U/2.0)*(n_wi*(n_wi-1)-n_i*(n_i-1)) - mu*(n_wi-n_i);
    dV_j = (U/2.0)*(n_wj*(n_wj-1)-n_j*(n_j-1)) - mu*(n_wj-n_j);
    
    // Calculate the weight ratio W'/W
    W = t * n_wj * exp((-dV_i+dV_j)*(tau_kink-tau_h));
    
    // Build the Metropolis ratio (R)
    p_dkah = 0.5;
    p_ikah = 0.5;
    R = W * (p_dkah/p_ikah) * (tau_max-tau_h)/p_site;
    
    // Metropolis Sampling
    if (rnum(rng) < R){ // Accept
        
        // Add to acceptance counter
        ikah_accepts += 1;
                
        // Change kink that stored head information to a regular kink
        paths[head_idx].tau = tau_kink;
        paths[head_idx].n = n_i;
        paths[head_idx].src = i;
        paths[head_idx].dest = j;
        paths[head_idx].prev = prev_i;
        paths[head_idx].next = next_i;
        
        // Create the kinks on the destination site
        paths[num_kinks]=Kink(tau_h,n_j,j,j,prev_j,num_kinks+1,
                              src_replica,dest_replica);
        paths[num_kinks+1]=Kink(tau_kink,n_wj,j,i,num_kinks,next_j,
                                src_replica,dest_replica);
        
        // Set new worm head index
        head_idx = num_kinks;
                
        // "Connect" next of lower bound kink to worm head
        paths[prev_j].next = head_idx;
        
        // "Connect" prev of next kink to new kink
        if(next_j!=-1){paths[next_j].prev = num_kinks+1;}
        
        // If new kink is last kink on site j, update last kinks tracker vector
        if (next_j==-1){last_kinks[j]=num_kinks+1;}
        
        // Update number of kinks tracker
        num_kinks += 2;

        return;
        }
        else // Reject
            return;
        }

/*----------------------------------------------------------------------------*/

void delete_kink_after_head(vector<Kink> &paths, int &num_kinks,
                int &head_idx,int &tail_idx,
                int M, int N, double U, double mu, double t,
                vector<vector<int>> &adjacency_matrix, int total_nn,
                double beta, double eta, bool canonical, double &N_tracker,
                int &N_zero, int &N_beta, vector<int> &last_kinks,
                unsigned long long int &dkah_attempts,
                unsigned long long int &dkah_accepts,
                boost::random::mt19937 &rng){
    
    // Variable declarations
    int prev,i,j,n_i,n_wi,n_j,n_wj,prev_i,prev_j,next_i,next_j,
    kink_idx_i,kink_idx_j,src_replica,dest_replica;
    double tau,tau_h,p_site,W,R,p_dkah,p_ikah,tau_prev_i,tau_prev_j,
    tau_kink,tau_max,dV_i,dV_j,tau_next_i,tau_next_j;
    
    // Update only possible if worm head present
    if (head_idx==-1){return;}
    
    // There has to be a regular kink after the worm head
    if (paths[head_idx].next==tail_idx ||
        paths[head_idx].next==-1){return;}

    // Need at least two sites to perform a spaceshift
    if (M<2){return;}

    // Indices of: upper bound kink, kink before head, lower bound kink ; site j
    kink_idx_j = paths[head_idx].next;
    next_j = paths[kink_idx_j].next;
    prev_j = paths[head_idx].prev;

    // Times of: worm head, kink before head, lower bound kink; site j
    if (next_j!=-1)
        tau_next_j = paths[next_j].tau;
    else
        tau_next_j = beta;
    tau_kink = paths[kink_idx_j].tau;
    tau_h = paths[head_idx].tau;
    tau_prev_j = paths[prev_j].tau;
    
    // Only kinks in which the particle hops from i TO j can be deleted
    if (paths[kink_idx_j].n-paths[head_idx].n<0){return;}
    
    // Retrieve worm head site (j) and connecting site (i)
    j = paths[head_idx].src;
    i = paths[kink_idx_j].dest;
    src_replica = paths[kink_idx_j].src_replica;
    dest_replica = paths[kink_idx_j].dest_replica;

    // Determine index of lower/upper bounds of flat where kink connects to (i)
    tau = 0.0;            // tau_prev_i candidate
    prev = i;           // prev_i candidate
    prev_i = i;         // this avoids "variable maybe not initialized" warning
    while (tau<tau_kink){
        // Set the lower bound index
        prev_i = prev;

        // Update lower bound index and tau candidates for next iteration
        prev = paths[prev].next;
        if (prev==-1){break;}
        tau = paths[prev].tau;
    }
    kink_idx_i = prev;
    next_i=paths[kink_idx_i].next;

    // Retrieve time of lower,upper bounds on connecting site (i)
    tau_prev_i = paths[prev_i].tau;
    if (next_i!=-1){tau_next_i = paths[next_i].tau;}
    else{tau_next_i=beta;}
    
    // Deletion cannot interfere w/ kinks on other site
    if (tau_h <= tau_prev_i){return;}

    // Add to proposal counter
    dkah_attempts += 1;

    // Determine highest time at which kink could've been inserted
    if (tau_next_i<tau_next_j){tau_max=tau_next_i;}
    else {tau_max=tau_next_j;}

    // Probability of inverse move (ikah) choosing site where worm end is
    p_site = 1.0/total_nn;

    // Extract no. of particles in the flats adjacent to the new kink
    n_wi = paths[prev_i].n;
    n_i = n_wi-1;
    n_wj = paths[prev_j].n;
    n_j = n_wj-1;                   // "w": segment with the extra particle

    // Calculate the diagonal energy difference on both sites
    dV_i = (U/2.0)*(n_wi*(n_wi-1)-n_i*(n_i-1)) - mu*(n_wi-n_i);
    dV_j = (U/2.0)*(n_wj*(n_wj-1)-n_j*(n_j-1)) - mu*(n_wj-n_j);

    // Calculate the weight ratio W'/W
    W = t * n_wj * exp((-dV_i+dV_j)*(tau_kink-tau_h));

    // Build the Metropolis ratio (R)
    p_dkah = 0.5;
    p_ikah = 0.5;
    R = W * (p_dkah/p_ikah) * (tau_max-tau_h)/p_site;
    R = 1.0/R;

    // Metropolis Sampling
    boost::random::uniform_real_distribution<double> rnum(0.0, 1.0);
    if (rnum(rng) < R){ // Accept

        // Add to acceptance counter
        dkah_accepts += 1;
        
        // Stage 1: Delete kink on i
        if (paths[num_kinks-1].next!=-1)
            paths[paths[num_kinks-1].next].prev = kink_idx_i;
        paths[paths[num_kinks-1].prev].next = kink_idx_i;
        
        swap(paths[kink_idx_i],paths[num_kinks-1]);
        
        if (prev_i==num_kinks-1){prev_i=kink_idx_i;}
        else if (next_i==num_kinks-1){next_i=kink_idx_i;}
        else if (prev_j==num_kinks-1){prev_j=kink_idx_i;}
        else if (next_j==num_kinks-1){next_j=kink_idx_i;}
        else if (kink_idx_j==num_kinks-1){kink_idx_j=kink_idx_i;}
        else if (head_idx==num_kinks-1){head_idx=kink_idx_i;}
        else {;}
        
        if (tail_idx==num_kinks-1){tail_idx=kink_idx_i;}
        
        if (paths[kink_idx_i].next==-1){
            last_kinks[paths[kink_idx_i].src]=kink_idx_i;
        }
        
        if (next_i!=-1)
            paths[next_i].prev = prev_i;
        paths[prev_i].next = next_i;
        
        if (next_i==-1){last_kinks[i]=prev_i;}

        // Stage 2: Delete kink on j
        if (paths[num_kinks-2].next!=-1)
            paths[paths[num_kinks-2].next].prev = kink_idx_j;
        paths[paths[num_kinks-2].prev].next = kink_idx_j;
        
        swap(paths[kink_idx_j],paths[num_kinks-2]);
        
        if (prev_i==num_kinks-2){prev_i=kink_idx_j;}
        else if (next_i==num_kinks-2){next_i=kink_idx_j;}
        else if (prev_j==num_kinks-2){prev_j=kink_idx_j;}
        else if (next_j==num_kinks-2){next_j=kink_idx_j;}
        else if (head_idx==num_kinks-2){head_idx=kink_idx_j;}
        else {;}
        
        if (tail_idx==num_kinks-2){tail_idx=kink_idx_j;}
        
        if (paths[kink_idx_j].next==-1){
            last_kinks[paths[kink_idx_j].src]=kink_idx_j;
        }
        
        if (next_j!=-1)
            paths[next_j].prev = head_idx;
        paths[head_idx].next = next_j;
        
        if (next_j==-1){last_kinks[j]=head_idx;}
        
        // Stage 3: Delete worm head on j
        if (paths[num_kinks-3].next!=-1)
            paths[paths[num_kinks-3].next].prev = head_idx;
        paths[paths[num_kinks-3].prev].next = head_idx;
        
        swap(paths[head_idx],paths[num_kinks-3]);
        
        if (prev_i==num_kinks-3){prev_i=head_idx;}
        else if (next_i==num_kinks-3){next_i=head_idx;}
        else if (prev_j==num_kinks-3){prev_j=head_idx;}
        else if (next_j==num_kinks-3){next_j=head_idx;}
        else {;}
        
        if (tail_idx==num_kinks-3){tail_idx=head_idx;}
        
        if (paths[head_idx].next==-1){
            last_kinks[paths[head_idx].src]=head_idx;
        }
        
        if (next_j!=-1)
            paths[next_j].prev = prev_j;
        paths[prev_j].next = next_j;
        
        if (next_j==-1){last_kinks[j]=prev_j;}
        
        // Stage 4: Insert worm head on i
        paths[num_kinks-3]=Kink(tau_h,n_i,i,i,prev_i,next_i,
                                src_replica,dest_replica);
        
        head_idx = num_kinks-3;
        
        paths[prev_i].next = head_idx;
        if(next_i!=-1){paths[next_i].prev = head_idx;}
        
        if (next_i==-1){last_kinks[i]=head_idx;}
        
        // Update number of kinks tracker
        num_kinks -= 2;

        return;

    }
    else // Reject
        return;
    }

/*----------------------------------------------------------------------------*/

void insert_kink_before_tail(vector<Kink> &paths, int &num_kinks,
                int &head_idx,int &tail_idx,
                int M, int N, double U, double mu, double t,
                vector<vector<int>> &adjacency_matrix, int total_nn,
                double beta, double eta, bool canonical, double &N_tracker,
                int &N_zero, int &N_beta, vector<int> &last_kinks,
                unsigned long long int &ikbt_attempts,
                unsigned long long int &ikbt_accepts,
                boost::random::mt19937 &rng){
    
    // Variable declarations
    int prev,i,j,n_i,n_wi,n_j,n_wj,prev_i,prev_j,next_i,next_j,
    src_replica,dest_replica;
    double tau,tau_t,p_site,W,R,p_dkbt,p_ikbt,tau_prev_i,tau_prev_j,
    tau_kink,tau_min,dV_i,dV_j,tau_next_i,tau_next_j;
    
    // Update only possible if worm tail present
    if (tail_idx==-1){return;}
    
    // Need at least two sites to perform a spaceshift
    if (M<2){return;}
    
    // Extract the worm tail site
    i = paths[tail_idx].src;
    src_replica = paths[tail_idx].src_replica;
    dest_replica = paths[tail_idx].dest_replica;
    
    // Randomly choose a nearest neighbor site
    boost::random::uniform_int_distribution<> random_nn(0, total_nn-1);
    j = adjacency_matrix[i][random_nn(rng)];
    p_site = 1.0/total_nn;
    
    // Retrieve the time of the worm tail
    tau_t = paths[tail_idx].tau;
    
    // Determine index of lower/upper kinks of flat where tail is (site i)
    prev_i = paths[tail_idx].prev;
    next_i = paths[tail_idx].next;
    
    // Determine index of lower/upper kinks of flat where tail jumps to (site j)
    tau = 0.0;            // tau_prev_j candidate
    prev = j;           // prev_j candidate
    prev_j = j;         // this avoids "variable maybe not initialized" warning
    while (tau<tau_t){
        // Set the lower bound index
        prev_j = prev;
        
        // Update lower bound index and tau candidates for next iteration
        prev = paths[prev].next;
        if (prev==-1){break;}
        tau = paths[prev].tau;
    }
    next_j=prev;
    
    // Determine upper,lower bound times on both sites
    tau_prev_i = paths[prev_i].tau;
    tau_prev_j = paths[prev_j].tau;
    if (next_i!=-1)
        tau_next_i = paths[next_i].tau;
    else
        tau_next_i = beta;
    if (next_j!=-1)
        tau_next_j = paths[next_j].tau;
    else
        tau_next_j = beta;
    
    // Determine lowest time at which kink could've been inserted
    if (tau_prev_i>tau_prev_j){tau_min=tau_prev_i;}
    else {tau_min=tau_prev_j;}
    
    // Randomly choose the time of the kink
    boost::random::uniform_real_distribution<double> rnum(0.0, 1.0);
    tau_kink = tau_min + rnum(rng)*(tau_t-tau_min);
    if (tau_kink == tau_min){return;}
    
     // Extract no. of particles in the flats adjacent to the new kink
     n_i = paths[prev_i].n;
     n_wi = n_i+1;
     n_wj = paths[prev_j].n;
     n_j = n_wj-1;                   // "w": segment with the extra particle
    
    // Update not possible if no particles on destinaton site (j)
    if (n_wj == 0){return;}
    
    // Add to proposal counter
    ikbt_attempts += 1;
    
    // Calculate the diagonal energy difference on both sites
    dV_i = (U/2.0)*(n_wi*(n_wi-1)-n_i*(n_i-1)) - mu*(n_wi-n_i);
    dV_j = (U/2.0)*(n_wj*(n_wj-1)-n_j*(n_j-1)) - mu*(n_wj-n_j);
    
    // Calculate the weight ratio W'/W
    W = t * n_wj * exp((-dV_i+dV_j)*(tau_t-tau_kink));

    // Build the Metropolis ratio (R)
    p_dkbt = 0.5;
    p_ikbt = 0.5;
    R = W * (p_dkbt/p_ikbt) * (tau_t-tau_min)/p_site;
    
    // Metropolis Sampling
    if (rnum(rng) < R){ // Accept
        
        // Add to acceptance counter
        ikbt_accepts += 1;
                
        // Change kink that stored tail information to a regular kink
        paths[tail_idx].tau = tau_kink;
        paths[tail_idx].n = n_wi;
        paths[tail_idx].src = i;
        paths[tail_idx].dest = j;
        paths[tail_idx].prev = prev_i;
        paths[tail_idx].next = next_i;
        
        // Create the kinks on the destination site
        paths[num_kinks]=Kink(tau_kink,n_j,j,i,prev_j,num_kinks+1,
                              src_replica,dest_replica);
        paths[num_kinks+1]=Kink(tau_t,n_wj,j,j,num_kinks,next_j,
                              src_replica,dest_replica);
        
        // Set new worm tail index
        tail_idx = num_kinks+1;
                
        // "Connect" next of lower bound kink to new kink
        paths[prev_j].next = num_kinks;
        
        // "Connect" prev of next kink to worm tail
        if(next_j!=-1){paths[next_j].prev = tail_idx;}
                
        // Update number of kinks tracker
        num_kinks += 2;
        
        // If worm tail is last kink on site j, update last kinks tracker vector
        if (next_j==-1){last_kinks[j]=tail_idx;}
        
        return;
            
        }
        else // Reject
            return;
        }

/*----------------------------------------------------------------------------*/

void delete_kink_before_tail(vector<Kink> &paths, int &num_kinks,
                int &head_idx,int &tail_idx,
                int M, int N, double U, double mu, double t,
                vector<vector<int>> &adjacency_matrix, int total_nn,
                double beta, double eta, bool canonical, double &N_tracker,
                int &N_zero, int &N_beta, vector<int> &last_kinks,
                unsigned long long int &dkbt_attempts,
                unsigned long long int &dkbt_accepts,
                boost::random::mt19937 &rng){
    
    // Variable declarations
    int prev,i,j,n_i,n_wi,n_j,n_wj,prev_i,prev_j,next_i,next_j,
    kink_idx_i,kink_idx_j,src_replica,dest_replica;
    double tau,tau_t,p_site,W,R,p_dkbt,p_ikbt,tau_prev_i,tau_prev_j,
    tau_kink,tau_min,dV_i,dV_j,tau_next_i;
    
    // Update only possible if worm tail present
    if (tail_idx==-1){return;}
    
    // There has to be a regular kink after the worm tail
    if (paths[tail_idx].prev==head_idx ||
        paths[paths[tail_idx].prev].tau==0){return;}

    // Need at least two sites to perform a spaceshift
    if (M<2){return;}
    
    // Indices of: upper bound kink, kink before tail, lower bound kink ; site j
    next_j = paths[tail_idx].next;
    kink_idx_j = paths[tail_idx].prev;
    prev_j = paths[kink_idx_j].prev;

    // Times of: worm tail, kink before tail, lower bound kink; site j
    tau_t = paths[tail_idx].tau;
    tau_kink = paths[kink_idx_j].tau;
    tau_prev_j = paths[prev_j].tau;
    
    // Only kinks in which the particle hops from j TO i can be deleted
    if (paths[kink_idx_j].n-paths[prev_j].n>0){return;}
    
    // Retrieve worm tail site (j) and connecting site (i)
    j = paths[kink_idx_j].src;
    i = paths[kink_idx_j].dest;
    src_replica = paths[kink_idx_j].src_replica;
    dest_replica = paths[kink_idx_j].dest_replica;
    
    // Determine index of lower/upper bounds of flat where kink connects to (i)
    tau = 0.0;            // tau_prev_i candidate
    prev = i;           // prev_i candidate
    prev_i = i;         // this avoids "variable maybe not initialized" warning
    while (tau<tau_kink){
        // Set the lower bound index
        prev_i = prev;

        // Update lower bound index and tau candidates for next iteration
        prev = paths[prev].next;
        if (prev==-1){break;}
        tau = paths[prev].tau;
    }
    kink_idx_i = prev;
    next_i=paths[kink_idx_i].next;

    // Retrieve time of lower,upper bounds on connecting site (i)
    tau_prev_i = paths[prev_i].tau;
    if (next_i==-1){tau_next_i=beta;}
    else {tau_next_i=paths[next_i].tau;};
    
    // Deletion cannot interfere w/ kinks on other site
    if (tau_t>=tau_next_i){return;}

    // Add to proposal counter
    dkbt_attempts += 1;

    // Determine lowest time at which kink could've been inserted
    if (tau_prev_i>tau_prev_j){tau_min=tau_prev_i;}
    else {tau_min=tau_prev_j;}
    
    // Probability of inverse move (ikbt) choosing site where worm end is
    p_site = 1.0/total_nn;

    // Extract no. of particles in the flats adjacent to the new kink
    n_i = paths[prev_i].n;
    n_wi = n_i+1;
    n_wj = paths[prev_j].n;
    n_j = n_wj-1;                   // "w": segment with the extra particle

    // Calculate the diagonal energy difference on both sites
    dV_i = (U/2.0)*(n_wi*(n_wi-1)-n_i*(n_i-1)) - mu*(n_wi-n_i);
    dV_j = (U/2.0)*(n_wj*(n_wj-1)-n_j*(n_j-1)) - mu*(n_wj-n_j);

    // Calculate the weight ratio W'/W
    W = t * n_wj * exp((-dV_i+dV_j)*(tau_t-tau_kink));

    // Build the Metropolis ratio (R)
    p_dkbt = 0.5;
    p_ikbt = 0.5;
    R = W * (p_dkbt/p_ikbt) * (tau_t-tau_min)/p_site;
    R = 1.0/R;
    
    // Metropolis Sampling
    boost::random::uniform_real_distribution<double> rnum(0.0, 1.0);
    if (rnum(rng) < R){ // Accept

        // Add to acceptance counter
        dkbt_accepts += 1;
        
        // Stage 1: Delete kink on i
        if (paths[num_kinks-1].next!=-1)
            paths[paths[num_kinks-1].next].prev = kink_idx_i;
        paths[paths[num_kinks-1].prev].next = kink_idx_i;
        
        swap(paths[kink_idx_i],paths[num_kinks-1]);
        
        if (prev_i==num_kinks-1){prev_i=kink_idx_i;}
        else if (next_i==num_kinks-1){next_i=kink_idx_i;}
        else if (prev_j==num_kinks-1){prev_j=kink_idx_i;}
        else if (next_j==num_kinks-1){next_j=kink_idx_i;}
        else if (kink_idx_j==num_kinks-1){kink_idx_j=kink_idx_i;}
        else if (tail_idx==num_kinks-1){tail_idx=kink_idx_i;}
        else {;}
        
        if (head_idx==num_kinks-1){head_idx=kink_idx_i;}
        
        if (paths[kink_idx_i].next==-1){
            last_kinks[paths[kink_idx_i].src]=kink_idx_i;
        }
        
        if (next_i!=-1)
            paths[next_i].prev = prev_i;
        paths[prev_i].next = next_i;
        
        if (next_i==-1){last_kinks[i]=prev_i;}

        // Stage 2: Delete worm tail on j
        if (paths[num_kinks-2].next!=-1)
            paths[paths[num_kinks-2].next].prev = tail_idx;
        paths[paths[num_kinks-2].prev].next = tail_idx;
        
        swap(paths[tail_idx],paths[num_kinks-2]);
        
        if (prev_i==num_kinks-2){prev_i=tail_idx;}
        else if (next_i==num_kinks-2){next_i=tail_idx;}
        else if (prev_j==num_kinks-2){prev_j=tail_idx;}
        else if (next_j==num_kinks-2){next_j=tail_idx;}
        else if (kink_idx_j==num_kinks-2){kink_idx_j=tail_idx;}
        else {;}
        
        if (head_idx==num_kinks-2){head_idx=tail_idx;}
        
        if (paths[tail_idx].next==-1){
            last_kinks[paths[tail_idx].src]=tail_idx;
        }
        
        if (next_j!=-1)
            paths[next_j].prev = kink_idx_j;
        paths[kink_idx_j].next = next_j;
        
        if (next_j==-1){last_kinks[j]=kink_idx_j;}
                
        // Stage 3: Delete kink on j
        if (paths[num_kinks-3].next!=-1)
            paths[paths[num_kinks-3].next].prev = kink_idx_j;
        paths[paths[num_kinks-3].prev].next = kink_idx_j;
        
        swap(paths[kink_idx_j],paths[num_kinks-3]);
        
        if (prev_i==num_kinks-3){prev_i=kink_idx_j;}
        else if (next_i==num_kinks-3){next_i=kink_idx_j;}
        else if (prev_j==num_kinks-3){prev_j=kink_idx_j;}
        else if (next_j==num_kinks-3){next_j=kink_idx_j;}
        else {;}
        
        if (head_idx==num_kinks-3){head_idx=kink_idx_j;}
        
        if (paths[kink_idx_j].next==-1){
            last_kinks[paths[kink_idx_j].src]=kink_idx_j;
        }
        
        if (next_j!=-1)
            paths[next_j].prev = prev_j;
        paths[prev_j].next = next_j;
        
        if (next_j==-1){last_kinks[j]=prev_j;}

        // Stage 4: Insert worm tail on i
        paths[num_kinks-3]=Kink(tau_t,n_wi,i,i,prev_i,next_i,
                                src_replica,dest_replica);

        tail_idx = num_kinks-3;

        paths[prev_i].next = tail_idx;
        if(next_i!=-1){paths[next_i].prev = tail_idx;}

        if (next_i==-1){last_kinks[i]=tail_idx;}

        // Update number of kinks tracker
        num_kinks -= 2;
        
        return;

    }
    else // Reject
        return;
    }

/*----------------------------------------------------------------------------*/

void insert_kink_after_tail(vector<Kink> &paths, int &num_kinks,
                int &head_idx,int &tail_idx,
                int M, int N, double U, double mu, double t,
                vector<vector<int>> &adjacency_matrix, int total_nn,
                double beta, double eta, bool canonical, double &N_tracker,
                int &N_zero, int &N_beta, vector<int> &last_kinks,
                unsigned long long int &ikat_attempts,
                unsigned long long int &ikat_accepts,
                boost::random::mt19937 &rng){
    
    // Variable declarations
    int prev,i,j,n_i,n_wi,n_j,n_wj,prev_i,prev_j,next_i,next_j,
    src_replica,dest_replica;
    double tau,tau_t,p_site,W,R,p_dkat,p_ikat,tau_prev_i,tau_prev_j,
    tau_kink,tau_max,dV_i,dV_j,tau_next_i,tau_next_j;
    
    // Update only possible if worm tail present
    if (tail_idx==-1){return;}
    
    // Need at least two sites to perform a spaceshift
    if (M<2){return;}
    
    // Add to proposal counter
    ikat_attempts += 1;
    
    // Extract the worm tail site
    i = paths[tail_idx].src;
    src_replica = paths[tail_idx].src_replica;
    dest_replica = paths[tail_idx].dest_replica;
    
    // Randomly choose a nearest neighbor site
    boost::random::uniform_int_distribution<> random_nn(0, total_nn-1);
    j = adjacency_matrix[i][random_nn(rng)];
    p_site = 1.0/total_nn;
    
    // Retrieve the time of the worm tail
    tau_t = paths[tail_idx].tau;
    
    // Determine index of lower/upper kinks of flat where tail is (site i)
    prev_i = paths[tail_idx].prev;
    next_i = paths[tail_idx].next;
    
    // Determine index of lower/upper kinks of flat where tail jumps to (site j)
    tau = 0.0;            // tau_prev_j candidate
    prev = j;           // prev_j candidate
    prev_j = j;         // this avoids "variable maybe not initialized" warning
    while (tau<tau_t){
        // Set the lower bound index
        prev_j = prev;
        
        // Update lower bound index and tau candidates for next iteration
        prev = paths[prev].next;
        if (prev==-1){break;}
        tau = paths[prev].tau;
    }
    next_j=prev;
    
    // Determine upper,lower bound times on both sites
    tau_prev_i = paths[prev_i].tau;
    tau_prev_j = paths[prev_j].tau;
    if (next_i!=-1)
        tau_next_i = paths[next_i].tau;
    else
        tau_next_i = beta;
    if (next_j!=-1)
        tau_next_j = paths[next_j].tau;
    else
        tau_next_j = beta;
    
    // Determine highest time at which kink could've been inserted
    if (tau_next_i<tau_next_j){tau_max=tau_next_i;}
    else {tau_max=tau_next_j;}
    
    // Randomly choose the time of the kink
    boost::random::uniform_real_distribution<double> rnum(0.0, 1.0);
    tau_kink = tau_t + rnum(rng)*(tau_max-tau_t);
    if (tau_kink==tau_t){return;}
    
     // Extract no. of particles in the flats adjacent to the new kink
     n_i = paths[prev_i].n;
     n_wi = n_i+1;
     n_j = paths[prev_j].n;
     n_wj = n_j+1;                   // "w": segment with the extra particle
    
    // Calculate the diagonal energy difference on both sites
    dV_i = (U/2.0)*(n_wi*(n_wi-1)-n_i*(n_i-1)) - mu*(n_wi-n_i);
    dV_j = (U/2.0)*(n_wj*(n_wj-1)-n_j*(n_j-1)) - mu*(n_wj-n_j);
    
    // Calculate the weight ratio W'/W
    W = t * n_wj * exp((dV_i-dV_j)*(tau_kink-tau_t));

    // Build the Metropolis ratio (R)
    p_dkat = 0.5;
    p_ikat = 0.5;
    R = W * (p_dkat/p_ikat) * (tau_max-tau_t)/p_site;
    
    // Metropolis Sampling
    if (rnum(rng) < R){ // Accept
        
        // Add to acceptance counter
        ikat_accepts += 1;
                
        // Change kink that stored tail information to a regular kink
        paths[tail_idx].tau = tau_kink;
        paths[tail_idx].n = n_wi;
        paths[tail_idx].src = i;
        paths[tail_idx].dest = j;
        paths[tail_idx].prev = prev_i;
        paths[tail_idx].next = next_i;
        
        // Create the kinks on the destination site
        paths[num_kinks]=Kink(tau_t,n_wj,j,j,prev_j,num_kinks+1,
                              src_replica,dest_replica);
        paths[num_kinks+1]=Kink(tau_kink,n_j,j,i,num_kinks,next_j,
                              src_replica,dest_replica);
        
        // Set new worm tail index
        tail_idx = num_kinks;
                
        // "Connect" next of lower bound kink to worm tail
        paths[prev_j].next = num_kinks;
        
        // "Connect" prev of next kink to new kink
        if(next_j!=-1){paths[next_j].prev = num_kinks+1;}
        
        // If new kink is last kink on site j, update last kinks tracker vector
        if (next_j==-1){last_kinks[j]=num_kinks+1;}
        
        // Update number of kinks tracker
        num_kinks += 2;
        
        return;
            
        }
        else // Reject
            return;
        }

/*----------------------------------------------------------------------------*/

void delete_kink_after_tail(vector<Kink> &paths, int &num_kinks,
                int &head_idx,int &tail_idx,
                int M, int N, double U, double mu, double t,
                vector<vector<int>> &adjacency_matrix, int total_nn,
                double beta, double eta, bool canonical, double &N_tracker,
                int &N_zero, int &N_beta, vector<int> &last_kinks,
                unsigned long long int &dkat_attempts,
                unsigned long long int &dkat_accepts,
                boost::random::mt19937 &rng){
    
    // Variable declarations
    int prev,i,j,n_i,n_wi,n_j,n_wj,prev_i,prev_j,next_i,next_j,
    kink_idx_i,kink_idx_j,src_replica,dest_replica;
    double tau,tau_t,p_site,W,R,p_dkat,p_ikat,tau_prev_i,tau_prev_j,
    tau_kink,tau_max,dV_i,dV_j,tau_next_i,tau_next_j;
    
    // Update only possible if worm tail present
    if (tail_idx==-1){return;}
    
    // There has to be a regular kink after the worm tail
    if (paths[tail_idx].next==head_idx ||
        paths[tail_idx].next==-1){return;}

    // Need at least two sites to perform a spaceshift
    if (M<2){return;}

    // Indices of: upper bound kink, kink before tail, lower bound kink ; site j
    kink_idx_j = paths[tail_idx].next;
    next_j = paths[kink_idx_j].next;
    prev_j = paths[tail_idx].prev;

    // Times of: worm tail, kink before tail, lower bound kink; site j
    if (next_j!=-1)
        tau_next_j = paths[next_j].tau;
    else
        tau_next_j = beta;
    tau_kink = paths[kink_idx_j].tau;
    tau_t = paths[tail_idx].tau;
    tau_prev_j = paths[prev_j].tau;
    
    // Only kinks in which the particle hops from j TO i can be deleted
    if ((paths[kink_idx_j].n-paths[tail_idx].n)>0){return;}
    
    // Retrieve worm tail site (j) and connecting site (i)
    j = paths[kink_idx_j].src;
    i = paths[kink_idx_j].dest;
    src_replica = paths[kink_idx_j].src_replica;
    dest_replica = paths[kink_idx_j].dest_replica;

    // Determine index of lower/upper bounds of flat where kink connects to (i)
    tau = 0.0;            // tau_prev_i candidate
    prev = i;           // prev_i candidate
    prev_i = i;         // this avoids "variable maybe not initialized" warning
    while (tau<tau_kink){
        // Set the lower bound index
        prev_i = prev;

        // Update lower bound index and tau candidates for next iteration
        prev = paths[prev].next;
        if (prev==-1){break;}
        tau = paths[prev].tau;
    }
    kink_idx_i = prev;
    next_i=paths[kink_idx_i].next;

    // Retrieve time of lower,upper bounds on connecting site (i)
    tau_prev_i = paths[prev_i].tau;
    if (next_i==-1){tau_next_i=beta;}
    else {tau_next_i = paths[next_i].tau;};
    
    // Deletion cannot interfere w/ kinks on other site
    if (tau_t <= tau_prev_i){return;}

    // Add to proposal counter
    dkat_attempts += 1;

    // Determine highest time at which kink could've been inserted
    if (tau_next_i<tau_next_j){tau_max=tau_next_i;}
    else {tau_max=tau_next_j;}

    // Probability of inverse move (ikah) choosing site where worm end is
    p_site = 1.0/total_nn;

    // Extract no. of particles in the flats adjacent to the new kink
    n_i = paths[prev_i].n;
    n_wi = n_i+1;
    n_j = paths[prev_j].n;
    n_wj = n_j+1;                   // "w": segment with the extra particle

    // Calculate the diagonal energy difference on both sites
    dV_i = (U/2.0)*(n_wi*(n_wi-1)-n_i*(n_i-1)) - mu*(n_wi-n_i);
    dV_j = (U/2.0)*(n_wj*(n_wj-1)-n_j*(n_j-1)) - mu*(n_wj-n_j);

    // Calculate the weight ratio W'/W
    W = t * n_wj * exp((dV_i-dV_j)*(tau_kink-tau_t));

    // Build the Metropolis ratio (R)
    p_dkat = 0.5;
    p_ikat = 0.5;
    R = W * (p_dkat/p_ikat) * (tau_max-tau_t)/p_site;
    R = 1.0/R;

    // Metropolis Sampling
    boost::random::uniform_real_distribution<double> rnum(0.0, 1.0);
    if (rnum(rng) < R){ // Accept

        // Add to acceptance counter
        dkat_accepts += 1;
        
        // Stage 1: Delete kink on i
        if (paths[num_kinks-1].next!=-1)
            paths[paths[num_kinks-1].next].prev = kink_idx_i;
        paths[paths[num_kinks-1].prev].next = kink_idx_i;
        
        swap(paths[kink_idx_i],paths[num_kinks-1]);
        
        if (prev_i==num_kinks-1){prev_i=kink_idx_i;}
        else if (next_i==num_kinks-1){next_i=kink_idx_i;}
        else if (prev_j==num_kinks-1){prev_j=kink_idx_i;}
        else if (next_j==num_kinks-1){next_j=kink_idx_i;}
        else if (kink_idx_j==num_kinks-1){kink_idx_j=kink_idx_i;}
        else if (tail_idx==num_kinks-1){tail_idx=kink_idx_i;}
        else {;}
        
        if (head_idx==num_kinks-1){head_idx=kink_idx_i;}
        
        if (paths[kink_idx_i].next==-1){
            last_kinks[paths[kink_idx_i].src]=kink_idx_i;
        }
        
        if (next_i!=-1)
            paths[next_i].prev = prev_i;
        paths[prev_i].next = next_i;
        
        if (next_i==-1){last_kinks[i]=prev_i;}

        // Stage 2: Delete kink on j
        if (paths[num_kinks-2].next!=-1)
            paths[paths[num_kinks-2].next].prev = kink_idx_j;
        paths[paths[num_kinks-2].prev].next = kink_idx_j;
        
        swap(paths[kink_idx_j],paths[num_kinks-2]);
        
        if (prev_i==num_kinks-2){prev_i=kink_idx_j;}
        else if (next_i==num_kinks-2){next_i=kink_idx_j;}
        else if (prev_j==num_kinks-2){prev_j=kink_idx_j;}
        else if (next_j==num_kinks-2){next_j=kink_idx_j;}
        else if (tail_idx==num_kinks-2){tail_idx=kink_idx_j;}
        else {;}
        
        if (head_idx==num_kinks-2){head_idx=kink_idx_j;}
        
        if (paths[kink_idx_j].next==-1){
            last_kinks[paths[kink_idx_j].src]=kink_idx_j;
        }
        
        if (next_j!=-1)
            paths[next_j].prev = tail_idx;
        paths[tail_idx].next = next_j;
        
        if (next_j==-1){last_kinks[j]=tail_idx;}
        
        // Stage 3: Delete worm head on j
        if (paths[num_kinks-3].next!=-1)
            paths[paths[num_kinks-3].next].prev = tail_idx;
        paths[paths[num_kinks-3].prev].next = tail_idx;
        
        swap(paths[tail_idx],paths[num_kinks-3]);
        
        if (prev_i==num_kinks-3){prev_i=tail_idx;}
        else if (next_i==num_kinks-3){next_i=tail_idx;}
        else if (prev_j==num_kinks-3){prev_j=tail_idx;}
        else if (next_j==num_kinks-3){next_j=tail_idx;}
        else {;}
        
        if (head_idx==num_kinks-3){head_idx=tail_idx;}
        
        if (paths[tail_idx].next==-1){
            last_kinks[paths[tail_idx].src]=tail_idx;
        }
        
        if (next_j!=-1)
            paths[next_j].prev = prev_j;
        paths[prev_j].next = next_j;
        
        if (next_j==-1){last_kinks[j]=prev_j;}
        
        // Stage 4: Insert worm tail on i
        paths[num_kinks-3]=Kink(tau_t,n_wi,i,i,prev_i,next_i,
                                src_replica,dest_replica);
        
        tail_idx = num_kinks-3;
        
        paths[prev_i].next = tail_idx;
        if(next_i!=-1){paths[next_i].prev = tail_idx;}
        
        if (next_i==-1){last_kinks[i]=tail_idx;}
        
        // Update number of kinks tracker
        num_kinks -= 2;
        
        return;

    }
    else // Reject
        return;
    }

/*----------------------------------------------------------------------------*/

void insert_swap_kink(vector<vector<Kink>> &paths,int &num_kinks,
                int num_replicas, int replica_idx,
                vector<int> &sub_sites, vector <int> &swapped_sites,
                vector<vector<int>> &swap_kinks,
                int l_A, int m_A,
                int &head_idx,int &tail_idx,
                int M, int N, double U, double mu, double t,
                vector<vector<int>> &adjacency_matrix, int total_nn,
                double beta, double eta, bool canonical, double &N_tracker,
                int &N_zero, int &N_beta, vector<int> &last_kinks,
                unsigned long long int &insert_swap_kink_attempts,
                unsigned long long int &insert_swap_kink_accepts,
                boost::random::mt19937 &rng){
    
    // ONE DIMENSIONAL FOR NOW

    // Variable declarations
    int prev,i,j,n_i,n_wi,n_j,n_wj,prev_i,prev_j,next_i,next_j,
    src_replica,dest_replica,site_R1,site_R2,site,swapped_site,n_src,n_dest,
    next;
    double tau,tau_t,p_site,W,R,p_dkat,p_ikat,tau_prev_i,tau_prev_j,
    tau_kink,tau_max,dV_i,dV_j,tau_next_i,tau_next_j,p_replica,p_swapped_site;
    int num_swaps;
    bool in_subsystem,is_unswapped,same_particles,is_swappable;
    vector<int> swappable_sites,nearest_neighbors;
    
    // Need at least two replicas to perform a spaceshift
    if (paths.size()<2){return;}
    
    num_swaps = static_cast<int>(swapped_sites.size());
    
    // Retrieve source replica index and randomly choose destination replica
    src_replica = replica_idx;
    if (num_replicas==2){
        if (src_replica==0){dest_replica=1;}
        else {dest_replica=0;}
        p_replica = 1.0;
    }
    else{ // more than two replica
        boost::random::uniform_real_distribution<double> rnum(0.0, 1.0);
        if (rnum(rng)<0.5){dest_replica = src_replica+1;}
        else {dest_replica = src_replica-1;}
        p_replica = 0.5;
        // Replica boundary conditions (periodic??)
        if (dest_replica!=-1 && dest_replica!=num_replicas){
            // nothing
        }
        else if (dest_replica==-1){dest_replica=num_replicas-1;}
        else {dest_replica=0;}
    }
    
    // Randomly choose a swapped lattice site in the subsystem
    if (num_swaps>0){
        boost::random::uniform_int_distribution<> sites(0, num_swaps-1);
        swapped_site = swapped_sites[sites(rng)];
        p_swapped_site = 1/num_swaps;
    }
    else { // no previously swapped sites. propose anywhere
//        boost::random::uniform_int_distribution<> sites(0, m_A-1);
//        swapped_site = sub_sites[sites(rng)];
//        p_site = 1/m_A;
    }

    // Extract the nearest neighbors of the chosen swapped site
    nearest_neighbors = adjacency_matrix[swapped_site];
    
    // Check which nearest neighbors satisfy swappable conditions
    for (int i=0; i<total_nn; i++){
        
        in_subsystem = false;
        if (std::find(sub_sites.begin(), sub_sites.end(), nearest_neighbors[i])
            != sub_sites.end()){in_subsystem=true;}
        
        is_unswapped = true;
        if  (std::find(swapped_sites.begin(),swapped_sites.end(),
                       nearest_neighbors[i])
             != swapped_sites.end()){is_unswapped=false;}
        
        // THIS PART COULD BE OMITTED BY KEEPING TRACK OF PARTICLE NUMBER AT
        // THE CENTRAL TIME SLICE AFTER EVERY UPDATE
        same_particles = false;
        // Source Replica
        tau = 0.0;
        next = nearest_neighbors[i];
        while (tau<0.5*beta){
            n_src = paths[src_replica][next].n;
            
            next = paths[src_replica][next].next;
            if (next==-1){break;}
            tau = paths[src_replica][next].tau;
        }
        // Destination Replica
        tau = 0.0;
        next = nearest_neighbors[i];
        while (tau<0.5*beta){
            n_dest = paths[dest_replica][next].n;
            
            next = paths[dest_replica][next].next;
            if (next==-1){break;}
            tau = paths[dest_replica][next].tau;
        }
        
        if (n_src==n_dest){same_particles=true;}
        
        // If tests passed. Add nearest neighbor to vector of swappable sites
        if (in_subsystem && is_unswapped && same_particles){
            swappable_sites.push_back(nearest_neighbors[i]);
        }
        
        
    }
    
    return;
}

/*----------------------------------------------------------------------------*/

void delete_swap_kink(vector<Kink> &paths, int &num_kinks,
                int &head_idx,int &tail_idx,
                int M, int N, double U, double mu, double t,
                vector<vector<int>> &adjacency_matrix, int total_nn,
                double beta, double eta, bool canonical, double &N_tracker,
                int &N_zero, int &N_beta, vector<int> &last_kinks,
                unsigned long long int &delete_swap_kink_attempts,
                unsigned long long int &delete_swap_kink_accepts,
                boost::random::mt19937 &rng){
    // swap kink update
    return;
}

/*------------------------------- Estimators ---------------------------------*/

// For diagonal estimators around a time slice, Fock State will be needed
void get_fock_state(double measurement_center, int M,
                    vector<int> &fock_state_at_slice,
                    vector<Kink> &paths){
    
    double tau;
    int current,n_i;
    
    for (int i=0; i<M; i++){
        current=i;
        tau = paths[current].tau;
        while (tau<=measurement_center && current!=-1){
            n_i=paths[current].n;
            fock_state_at_slice[i]=n_i;
            
            current=paths[current].next;
            if (current!=-1)
                tau=paths[current].tau;
        }
    }
    return;
}

/*----------------------------------------------------------------------------*/

vector<double> get_measurement_centers(double beta){
    
    double tau_center;
    vector<double> measurement_centers;
    int num_centers;
    
    num_centers=25; // use odd number please.
    tau_center=beta/(2*num_centers);
    for (int i=0; i<num_centers; i++){
        measurement_centers.push_back(tau_center);
        tau_center+=(beta/(num_centers));
    }
    return measurement_centers;
}

/*-------------------------------- Diagonal ----------------------------------*/

double pimc_diagonal_energy(vector<int> &fock_state_at_slice, int M,
                            bool canonical, double U, double mu){
    
    double diagonal_energy;
    int n_i;
        
    diagonal_energy=0.0;
    for (int i=0; i<M; i++){
        n_i = fock_state_at_slice[i];
        if (canonical)
            diagonal_energy += (U/2.0*n_i*(n_i-1));
        else
            diagonal_energy += (U/2.0*n_i*(n_i-1)-mu*n_i);
    }
    return diagonal_energy;
}

/*----------------------------------------------------------------------------*/

void tau_resolved_diagonal_energy(vector<Kink> &paths,
                                           int num_kinks, int M, bool canonical,
                                           double U, double mu, double beta,
                                           vector<double> &measurement_centers,
                                           vector<double> &tr_diagonal_energy){
    double tau,measurement_center;
    int current,n_i;
    
    for (int i=0; i<M; i++){
        current=i;
        tau=paths[current].tau;
        n_i=paths[current].n;
        for (int j=0; j<measurement_centers.size(); j++){
            measurement_center=measurement_centers[j];
            while (tau<=measurement_center && current!=-1){
                n_i=paths[current].n;
                
                current=paths[current].next;
                if (current!=-1)
                    tau=paths[current].tau;
            }
            if (canonical)
                tr_diagonal_energy[j]+=(U/2.0*n_i*(n_i-1.0));
            else
                tr_diagonal_energy[j]+=(U/2.0*n_i*(n_i-1.0)-mu*n_i);
        }
    }
    
    return;
}

/*------------------------------ Off-Diagonal --------------------------------*/

double pimc_kinetic_energy(vector<Kink> &paths, int num_kinks,
                      double measurement_center,double measurement_plus_minus,
                      int M, double t, double beta){
    
    int kinks_in_window=0;
    
    for (int k=0; k<num_kinks; k++){
        if (paths[k].tau>=measurement_center-measurement_plus_minus
            && paths[k].tau<=measurement_center+measurement_plus_minus){
            kinks_in_window+=1;
        }
    }
    
    return (-t*kinks_in_window/2.0)/(2.0*measurement_plus_minus);
}

/*----------------------------------------------------------------------------*/

void tau_resolved_kinetic_energy(vector<Kink> &paths,
                                           int num_kinks, int M,
                                           double t, double beta,
                                           vector<double> &measurement_centers,
                                           vector<double> &tr_kinetic_energy){
    double tau,measurement_center,window_width;

    window_width = measurement_centers[2]-measurement_centers[1];
    
    for (int i=M; i<num_kinks; i++){ // Note: the tau=0 kinks not counted
        tau = paths[i].tau;

        for (int j=0; j<measurement_centers.size(); j++){
            measurement_center=measurement_centers[j];

            if (tau>=measurement_center-window_width/2.0 &&
                tau<measurement_center+window_width/2.0){
                // add kink to bin
                tr_kinetic_energy[j]+=(-1.0/(2.0*window_width));
                break;
            }
        }
    }
    return;
}
/*----------------------------------------------------------------------------*/

#endif /* pimc_hpp */
