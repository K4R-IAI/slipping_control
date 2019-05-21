/*
    helper functions for slipping_control

    Copyright 2018 Università della Campania Luigi Vanvitelli

    Author: Marco Costanzo <marco.costanzo@unicampania.it>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "slipping_control_common/functions.h"


using namespace std;
using namespace TooN;

double pow_signed( double x, double exponent )
{
    return sign(x)*pow(abs(x), exponent);
}

double d_pow_signed( double x, double exponent )
{
    return exponent*pow(abs(x), exponent - 1.0);
}

void computeLSInfo(LS_INFO& ls_info)
{
    ls_info.xik_nuk = getXikNuk(ls_info.k);
    ls_info.alpha = computeAlpha( ls_info );
}

double getXikNuk(double k){
    if(isinf(k)){
        return 1.0/3.0;
    }
    if( k == 0 ){
        return 0.0;
    }
    return (3.0/8.0)  * pow(tgamma(3.0/k),2) / (  tgamma(2.0/k) * tgamma(4.0/k) );
}

double computeAlpha( const LS_INFO& ls_info )
{
    return 2.0*ls_info.mu*ls_info.xik_nuk*ls_info.delta;
}

double calculateSigma( double ft, double taun, const LS_INFO& ls_info )
{
    return ( ( 2.0 * ls_info.xik_nuk * ls_info.delta / pow(ls_info.mu, ls_info.gamma ) ) * pow_signed( ft, ls_info.gamma + 1.0 ) / taun ) ;
}

double calculateSigma( double ft_tilde, double taun_tilde, double gamma )
{
    return ( pow_signed( ft_tilde, gamma + 1.0 ) / taun_tilde );
}

double getMaxFt( double fn, double mu )
{
    return mu*fn;
}

double getMaxTaun( double fn , const LS_INFO& ls_info, double& radius )
{
    radius = getRadius(fn, ls_info);
    return 2.0*ls_info.mu*ls_info.xik_nuk*fn*radius;
}

double getMaxTaun( double fn , const LS_INFO& ls_info )
{
    return 2.0*ls_info.mu*ls_info.xik_nuk*fn*getRadius(fn, ls_info);
}

double getRadius( double fn , const LS_INFO& ls_info )
{
    return ls_info.delta*pow(fn, ls_info.gamma );
}
//SIGMA_INFO __SIGMA_INFO__;
//GAUSS_INFO __GAUSS_INFO__;
double computeCOR_tilde(    
                        double sigma, 
                        double gamma,
                        bool& b_max_iter, 
                        double initial_point,
                        double MAX_SIGMA, 
                        double GD_GAIN, 
                        double GD_COST_TOL, 
                        double FIND_ZERO_LAMBDA,
                        int MAX_GD_ITER
                        )
{
    //Numeric Fix
    if( (isinf(sigma) && sigma>0.0) || sigma > fabs(MAX_SIGMA) ){
        sigma = fabs(MAX_SIGMA);
    } else if( (isinf(sigma) && sigma<0.0) || sigma < -fabs(MAX_SIGMA) ) {
        sigma = -fabs(MAX_SIGMA);
    }
    if(initial_point == 0.0){
        initial_point = 1.0;
    }

    const boost::function<double(double)> ft_tilde_ls_boost = 
                                                            boost::bind(
                                                                        ft_tilde_ls_model,
                                                                        _1,
                                                                        boost::ref( __SIGMA_INFO__ )
                                                                        );

    const boost::function<double(double)> d_ft_tilde_ls_boost =
                                                            boost::bind(
                                                                        d_ft_tilde_ls_model,
                                                                        _1,
                                                                        boost::ref( __SIGMA_INFO__ )
                                                                        );

    const boost::function<double(double)> taun_tilde_ls_boost =
                                                            boost::bind(
                                                                        taun_tilde_ls_model,
                                                                        _1,
                                                                        boost::ref( __GAUSS_INFO__ )
                                                                        );

    const boost::function<double(double)> d_taun_tilde_ls_boost =
                                                            boost::bind(
                                                                        d_taun_tilde_ls_model,
                                                                        _1,
                                                                        boost::ref( __GAUSS_INFO__ )
                                                                        );

    return findZero(    
                            initial_point,
                            boost::bind(
                                        c_tilde_J_zero, 
                                        _1, 
                                        sigma,
                                        gamma, 
                                        ft_tilde_ls_boost,  
                                        taun_tilde_ls_boost
                            ), 
                            boost::bind(
                                        c_tilde_grad_J_zero, 
                                        _1, 
                                        sigma,
                                        gamma,
                                        ft_tilde_ls_boost,
                                        d_ft_tilde_ls_boost,
                                        taun_tilde_ls_boost,
                                        d_taun_tilde_ls_boost
                            ), 
                            GD_GAIN, 
                            GD_COST_TOL, 
                            FIND_ZERO_LAMBDA, 
                            MAX_GD_ITER, 
                            b_max_iter 
                            );

}

double c_tilde_J_zero(  
                        double c_tilde, 
                        double sigma,
                        double gamma, 
                        const boost::function<double(double)>& ft_tilde_ls_model_fcn,  
                        const boost::function<double(double)>& taun_tilde_ls_model_fcn 
                      )
{

    return ( sigma - (pow_signed( ft_tilde_ls_model_fcn(c_tilde), gamma + 1.0 ) / taun_tilde_ls_model_fcn(c_tilde) ) );

}

double c_tilde_grad_J_zero(  
                        double c_tilde, 
                        double sigma,
                        double gamma, 
                        const boost::function<double(double)>& ft_tilde_ls_model_fcn,
                        const boost::function<double(double)>& d_ft_tilde_ls_model_fcn,  
                        const boost::function<double(double)>& taun_tilde_ls_model_fcn,
                        const boost::function<double(double)>& d_taun_tilde_ls_model_fcn 
                      )
{

    double ft_tilde_ls = ft_tilde_ls_model_fcn(c_tilde);
    double d_ft_tilde_ls = d_ft_tilde_ls_model_fcn(c_tilde);
    double taun_tilde_ls = taun_tilde_ls_model_fcn(c_tilde);
    double d_taun_tilde_ls = d_taun_tilde_ls_model_fcn(c_tilde);

    double numerator = d_pow_signed( ft_tilde_ls, gamma+1.0 )*d_ft_tilde_ls*taun_tilde_ls  - pow_signed( ft_tilde_ls, gamma+1.0 )*d_taun_tilde_ls;

    return -( numerator/pow(taun_tilde_ls,2) );

}

double ft_tilde_ls_model( double c_tilde, const SIGMA_INFO& info )
{

    double ft_tilde = 0.0;
    for( int i=0; i<info.num_sigm; i++ ){
        ft_tilde += sigm_fun( c_tilde, info.gain[i], info.exponent[i], info.mean[i] );
    }

    return ft_tilde;

}

double sigm_fun( double x, double gain, double exponent, double mean )
{

    return gain*( 2.0/( 1.0 + exp( -exponent*( x - mean ) ) ) - 1.0 );

}

double d_ft_tilde_ls_model( double c_tilde, const SIGMA_INFO& info )
{

    double d_ft_tilde = 0.0;
    for( int i=0; i<info.num_sigm; i++ ){
        d_ft_tilde += d_sigm_fun( c_tilde, info.gain[i], info.exponent[i], info.mean[i] );
    }

    return d_ft_tilde;

}

double d_sigm_fun( double x, double gain, double exponent, double mean )
{

    return 2.0*gain*exponent*( exp( -exponent*( x - mean ) ) /pow( 1.0 + exp( -exponent*( x - mean ) ), 2 ) );

}

double taun_tilde_ls_model( double c_tilde, const GAUSS_INFO& info )
{

    double taun_tilde = 0.0;
    for( int i=0; i<info.num_gauss; i++ ){
        taun_tilde += gauss_fun( c_tilde, info.gain[i], info.mean[i], info.sigma_square[i] );
    }

    return taun_tilde;

}

double gauss_fun( double x, double gain, double mean, double sigma_square )
{
    return gain*exp( -pow( x - mean, 2 )/(2.0*sigma_square) );
}

double d_taun_tilde_ls_model( double c_tilde, const GAUSS_INFO& info )
{

    double d_taun_tilde = 0.0;
    for( int i=0; i<info.num_gauss; i++ ){
        d_taun_tilde += d_gauss_fun( c_tilde, info.gain[i], info.mean[i], info.sigma_square[i] );
    }

    return d_taun_tilde;

}

double d_gauss_fun( double x, double gain, double mean, double sigma_square )
{
    return -gain*((x - mean)/sigma_square)*exp( -pow( x - mean, 2 )/(2.0*sigma_square) );
}

double getFn_ls( double ft, double taun, double ft_tilde_ls, double taun_tilde_ls, const LS_INFO& ls_info )
{
    if(ft_tilde_ls > 0.1){
        return (ft/ls_info.mu)/fabs(ft_tilde_ls);
    } else if(taun_tilde_ls > 0.1){
        return pow( (taun/(2.0*ls_info.mu*ls_info.xik_nuk*ls_info.delta))/taun_tilde_ls , 1.0/(ls_info.gamma + 1.0) );
    } else {
        //Unable to compute Fn_ls... something goes wrong...
        //Maybe no object is grasped?
        return 0.0; //I don't know if it is a good value here...
    }
}

void initLS_model( double k, const string& folder )
{

    //Build Folder Name
    ostringstream streamObj;
    // Set Fixed -Point Notation
    streamObj << std::fixed;
    // Set precision to n digits
    streamObj << std::setprecision(INIT_LS_MODEL_FILE_NAME_DIGIT_PRECISION);
    //Add double to stream
    streamObj << k;
    // Get string from output string stream
    string model_file = streamObj.str();
    boost::replace_all(model_file, ".", "_");
    model_file = folder + model_file + MODEL_FILE_EXT;
    //DEBUG
    cout << "Model_File = " << model_file << endl;

    //Open File
    FILE *f;
	f = fopen(model_file.c_str(), "r");

    //Chek exist...
	if (f == NULL){
		printf(BOLDRED "Error opening file..." CRESET);
		printf(BOLDBLUE " %s\n" CRESET,model_file.c_str());
        printf(BOLDBLUE " Does the file exist?\n" CRESET);
		exit(-1);
	}

    //Read First double -> k
    double k_file;
    fscanf(f, "%lf,", &k_file);
    if(k_file!=k){
        //There is a problem... it is not the correct file?
        cout << BOLDRED "k does not match... " CRESET << k << "!=" << k_file << endl;
        fclose(f);
        exit(-1);
    }
cout << "ft" << endl;
    //Read ft_model
    double tmp;
    fscanf(f, "%lf,", &tmp);
    __SIGMA_INFO__.num_sigm = (int)tmp; cout << __SIGMA_INFO__.num_sigm << endl; 
    __SIGMA_INFO__.gain.clear();
    __SIGMA_INFO__.exponent.clear();
    __SIGMA_INFO__.mean.clear();
    for( int i=0; i<__SIGMA_INFO__.num_sigm; i++ ){
        fscanf(f, "%lf,", &tmp); cout << tmp << endl;
        __SIGMA_INFO__.gain.push_back(tmp);
        fscanf(f, "%lf,", &tmp); cout << tmp << endl;
        __SIGMA_INFO__.mean.push_back(tmp);
        fscanf(f, "%lf,", &tmp); cout << tmp << endl;
        __SIGMA_INFO__.exponent.push_back(tmp);
    }
cout << "taun" << endl;
    //Read taun_model
    fscanf(f, "%lf,", &tmp); 
    __GAUSS_INFO__.num_gauss = (int)tmp; cout << __GAUSS_INFO__.num_gauss << endl;
    __GAUSS_INFO__.gain.clear();
    __GAUSS_INFO__.mean.clear();
    __GAUSS_INFO__.sigma_square.clear();
    for( int i=0; i<__GAUSS_INFO__.num_gauss; i++ ){
        fscanf(f, "%lf,", &tmp); cout << tmp << endl;
        __GAUSS_INFO__.gain.push_back(tmp);
        fscanf(f, "%lf,", &tmp); cout << tmp << endl;
        __GAUSS_INFO__.mean.push_back(tmp);
        fscanf(f, "%lf,", &tmp); cout << tmp << endl;
        __GAUSS_INFO__.sigma_square.push_back(pow(tmp,2));
    }
cout << "DONE" << endl;
    fclose(f);

}










//------------------------------------ OLD


double min_force_Jcst( double fn, const boost::function<double(double)>& limitSurface, double ft, double taun ,const LS_INFO& info ){

	fn = fabs(fn);
    if(fn == 0.0)
        return 10.0;

    double ft_norm = ft/(info.gamma*fn);
    if(ft_norm > 1.0)
        ft_norm = 1.0;
    

    return ( limitSurface( ft_norm ) - (1.0/info.alpha) * taun / pow(fn,1.0+info.gamma) );

}

double min_force_gradJ( double fn, const boost::function<double(double)>& diff_limitSurface, double ft, double taun ,const LS_INFO& info  ){


    double segn;
    if(fn < 0.0)
        segn = -1.0;
    else if(fn > 0.0)
        segn = 1.0;
    else
        return -1;
        
    fn = fabs(fn);
    
    double ft_norm = ft/(info.gamma*fn);
    if(ft_norm < 1.0)
        return segn*( -diff_limitSurface( ft_norm )*ft/(info.gamma*pow(fn,2))  + (1.0/info.alpha)*taun*(1.0+info.gamma)/pow(fn,info.gamma+2.0) );
    else
        return segn*( (1.0/info.alpha)*taun*(1.0+info.gamma)/pow(fn,info.gamma+2.0) );

}

double limitSurface_true( double fnk ){
    return polyval( __ls_vector__, fnk  );    
}

double diff_limitSurface_true( double fnk ){
    return polyval( __diff_ls_vector__, fnk  );
}


double limitSurface_line( double ft_n ){
    return ( -ft_n + 1 );
}

double diff_limitSurface_line( double ft_n ){
    return -1;
}

void initANN_COR_R(){
    string path("");
    path = ros::package::getPath("slipping_control_common");
    path += "/ANN_COR";

    __ann_COR_R__ = new ANN ( path );
}

Vector<> vel_sys_h_fcn(const Vector<>& x, const Vector<>& u, const VEL_SYSTEM_INFO& info){
    Vector<2> ret = makeVector( info.sigma_02*x[1] + info.beta_o2*x[0], info.sigma_03*x[2] + info.beta_o3*x[0]);
    return ret;
}

Matrix<> vel_sys_HH_fcn(const Vector<>& x, const Vector<>& u, const VEL_SYSTEM_INFO& info){
    Matrix<2,3> ret = Data( info.beta_o2 , info.sigma_02, 0.0, info.beta_o3, 0.0, info.sigma_03 );
    return ret;
}

Vector<> vel_sys_f_fcn_cont(const Vector<>& x, const Vector<>& u, const VEL_SYSTEM_INFO& info){
    
    Vector<3> x_dot = Zeros;

    double den = 1.0/( info.Io + info.Mo*pow(info.cor+info.b,2) );

    x_dot[0] = den * ( -(info.beta_o2 + info.beta_o3)*x[0] - info.sigma_02*x[1] - info.sigma_03*x[2] + u[0] );

    x_dot[1] = x[0] - info.sigma_02/info.f_max_0 * fabs(x[0]) * x[1];

    x_dot[2] = x[0] - info.sigma_03/info.f_max_1 * fabs(x[0]) * x[2];

    return x_dot;

}

Matrix<> vel_sys_FF_fcn_cont(const Vector<>& x, const Vector<>& u, const VEL_SYSTEM_INFO& info){
    
    Matrix<3,3> F = Zeros;

    double den = 1.0/( info.Io + info.Mo*pow(info.cor+info.b,2) );

    F(0,0) = -den * ( info.beta_o2 + info.beta_o3 );

    F(0,1) = -info.sigma_02*den;

    F(0,2) = -info.sigma_03*den;

    F(1,0) = 1.0 - info.sigma_02/info.f_max_0 * x[1] * sign( x[0] );

    F(1,1) = -info.sigma_02/info.f_max_0 * fabs( x[0] );

    F(1,2) = 0.0;

    F(2,0) = 1.0 - info.sigma_03/info.f_max_1 * x[2] * sign( x[0] );

    F(2,1) = 0.0;

    F(2,2) = -info.sigma_03/info.f_max_1 * fabs( x[0] );

    return F;

}

//NUOVO SISTEMA TRANSL

Vector<> vel_sys_transl_f_fcn_cont(const Vector<>& x, const Vector<>& u, const VEL_SYSTEM_INFO& info){
    
    Vector<3> x_dot = Zeros;

    double den = 1.0/info.Mo;

    x_dot[0] = den * ( -(info.beta_o2 + info.beta_o3)*x[0] - info.sigma_02*x[1] - info.sigma_03*x[2] + u[0] );

    x_dot[1] = x[0] - info.sigma_02/info.f_max_0 * fabs(x[0]) * x[1];

    x_dot[2] = x[0] - info.sigma_03/info.f_max_1 * fabs(x[0]) * x[2];

    return x_dot;

}

Matrix<> vel_sys_transl_FF_fcn_cont(const Vector<>& x, const Vector<>& u, const VEL_SYSTEM_INFO& info){
    
    Matrix<3,3> F = Zeros;

    double den = 1.0/info.Mo;

    F(0,0) = -den * ( info.beta_o2 + info.beta_o3 );

    F(0,1) = -info.sigma_02*den;

    F(0,2) = -info.sigma_03*den;

    F(1,0) = 1.0 - info.sigma_02/info.f_max_0 * x[1] * sign( x[0] );

    F(1,1) = -info.sigma_02/info.f_max_0 * fabs( x[0] );

    F(1,2) = 0.0;

    F(2,0) = 1.0 - info.sigma_03/info.f_max_1 * x[2] * sign( x[0] );

    F(2,1) = 0.0;

    F(2,2) = -info.sigma_03/info.f_max_1 * fabs( x[0] );

    return F;

}