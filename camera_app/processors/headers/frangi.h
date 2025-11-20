#include <opencv2/opencv.hpp>


//options for the filter
typedef struct{
	//vessel scales
	float sigma_start;
	float sigma_end;
	float sigma_step;
	
	//Beta: suppression of blob-like structures. 
	//C: background suppression. (See Frangi1998...)
	float Beta;
	float C;

	bool BlackWhite; // enhance black structures if true, otherwise enhance white structures
	bool auto_compute_c; // c is the half the value of the maximum Hessian norm or 75 percentile or just value
	
	bool use_percentile; //  use percentile_value (75.0 by default) percentile instead of the maximum Hessian norm or 75 percentile
	double percentile_value; 
} frangi2d_opts_t;

#define DEFAULT_SIGMA_START 0.5
#define DEFAULT_SIGMA_END 3.5
#define DEFAULT_SIGMA_STEP 0.5
#define DEFAULT_BETA 1.6
#define DEFAULT_C 0.08
#define DEFAULT_BLACKWHITE true
#define DEFAULT_AUTOCOMPUTE_C true
#define DEFAULT_USE_PERCENTILE true
#define DEFAULT_PERCENTILE_VALUE 75.0

/////////////////
//Frangi filter//
/////////////////

//apply full Frangi filter to src. Vesselness is saved in J, scale is saved to scale, vessel angle is saved to directions. 
void frangi2d(const cv::Mat &src, cv::Mat &J, cv::Mat &scale, cv::Mat &directions, frangi2d_opts_t opts);

////////////////////
//Helper functions//
////////////////////

//run 2D Hessian filter with parameter sigma on src, save to Dxx, Dxy and Dyy. 
void frangi2d_hessian(const cv::Mat &src, cv::Mat &Dxx, cv::Mat &Dxy, cv::Mat &Dyy, float sigma);

//set opts to default options (sigma_start = 3, sigma_end = 7, sigma_step = 1, BetaOne = 1.6, BetaTwo 0.08)
void frangi2d_createopts(frangi2d_opts_t *opts);

//estimate eigenvalues from Dxx, Dxy, Dyy. Save results to lambda1, lambda2, Ix, Iy. 
void frangi2_eig2image(const cv::Mat &Dxx, const cv::Mat &Dxy, const cv::Mat &Dyy, cv::Mat &lambda1, cv::Mat &lambda2, cv::Mat &Ix, cv::Mat &Iy);

// get percentile value instead of the maximum Hessian value
void frangi2d_compute_c_percentile(
    const std::vector<cv::Mat>& ALLlambda1, 
    const std::vector<cv::Mat>& ALLlambda2, 
    double& c, 
    double percentile = 75.0
);

