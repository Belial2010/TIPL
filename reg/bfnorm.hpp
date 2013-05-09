#ifndef BFNORM_HPP
#define BFNORM_HPP

#include <cmath>
#include <vector>
#include "image/numerical/matrix.hpp"
namespace image{

namespace reg{

template<typename ImageType>
double resample_d(const ImageType& vol,double& gradx,double& grady,double& gradz,double x,double y,double z)
{
    const double TINY = 5e-2;
    int xdim = vol.width();
    int ydim = vol.height();
    int zdim = vol.depth();
        {
                double xi,yi,zi;
                xi=x-1.0;
                yi=y-1.0;
                zi=z-1.0;
                if (zi>=-TINY && zi<zdim+TINY-1 &&
                    yi>=-TINY && yi<ydim+TINY-1 &&
                    xi>=-TINY && xi<xdim+TINY-1)
                {
                        double k111,k112,k121,k122,k211,k212,k221,k222;
                        double dx1, dx2, dy1, dy2, dz1, dz2;
                        int off1, off2, offx, offy, offz, xcoord, ycoord, zcoord;

                        xcoord = (int)floor(xi); dx1=xi-xcoord; dx2=1.0-dx1;
                        ycoord = (int)floor(yi); dy1=yi-ycoord; dy2=1.0-dy1;
                        zcoord = (int)floor(zi); dz1=zi-zcoord; dz2=1.0-dz1;

                        xcoord = (xcoord < 0) ? ((offx=0),0) : ((xcoord>=xdim-1) ? ((offx=0),xdim-1) : ((offx=1   ),xcoord));
                        ycoord = (ycoord < 0) ? ((offy=0),0) : ((ycoord>=ydim-1) ? ((offy=0),ydim-1) : ((offy=xdim),ycoord));
                        zcoord = (zcoord < 0) ? ((offz=0),0) : ((zcoord>=zdim-1) ? ((offz=0),zdim-1) : ((offz=1   ),zcoord));

                        off1 = xcoord  + xdim*ycoord;
                        off2 = off1+offy;
                        int z_offset = zcoord*vol.plane_size();
                        int z_offset_off = z_offset + (offz == 1 ? vol.plane_size():0);
                        k222 = vol[z_offset+off1]; k122 = vol[z_offset+off1+offx];
                        k212 = vol[z_offset+off2]; k112 = vol[z_offset+off2+offx];
                        k221 = vol[z_offset_off+off1]; k121 = vol[z_offset_off+off1+offx];
                        k211 = vol[z_offset_off+off2]; k111 = vol[z_offset_off+off2+offx];

                        gradx = (((k111 - k211)*dy1 + (k121 - k221)*dy2))*dz1
                                 + (((k112 - k212)*dy1 + (k122 - k222)*dy2))*dz2;

                        k111 = (k111*dx1 + k211*dx2);
                        k121 = (k121*dx1 + k221*dx2);
                        k112 = (k112*dx1 + k212*dx2);
                        k122 = (k122*dx1 + k222*dx2);

                        grady = (k111 - k121)*dz1 + (k112 - k122)*dz2;

                        k111 = k111*dy1 + k121*dy2;
                        k112 = k112*dy1 + k122*dy2;

                        gradz = k111 - k112;
                        return k111*dz1 + k112*dz2;
                }
                else
                {
                        gradx = 0.0;
                        grady = 0.0;
                        gradz = 0.0;
                        return 0;
                }
        }
}


template<typename value_type,int dim = 3>
class bfnorm_mapping{
public:
    image::geometry<dim> VGgeo;
    image::geometry<dim> k_base;
    std::vector<value_type> T;
    std::vector<std::vector<value_type> > bas,dbas;
public:
    bfnorm_mapping(const image::geometry<3>& geo_,const image::geometry<dim>& k_base_):VGgeo(geo_),k_base(k_base_)
    {
        //void initialize_basis_function(double stabilise) // bounding offset
        value_type stabilise = 8;
        bas.resize(dim);
        dbas.resize(dim);
        for(int d = 0;d < dim;++d)
        {
            value_type pi_inv_mni_dim = 3.14159265358979323846/(double)VGgeo[d];
            bas[d].resize(VGgeo[d]*k_base[d]);
            dbas[d].resize(VGgeo[d]*k_base[d]);
            // C(:,1)=ones(size(n,1),1)/sqrt(N);
            std::fill(bas[d].begin(),bas[d].begin()+VGgeo[d],stabilise/std::sqrt((float)VGgeo[d]));
            std::fill(dbas[d].begin(),dbas[d].begin()+VGgeo[d],0.0);
            for(int i = 1,index = VGgeo[d];i < k_base[d];++i)
            for(int n = 0;n < VGgeo[d];++n,++index)
            {
                // C(:,k) = sqrt(2/N)*cos(pi*(2*n+1)*(k-1)/(2*N));
                bas[d][index] = stabilise*std::sqrt(2.0/(double)VGgeo[d])*std::cos(pi_inv_mni_dim*(value_type)i*((value_type)n+0.5));
                // C(:,k) = -2^(1/2)*(1/N)^(1/2)*sin(1/2*pi*(2*n*k-2*n+k-1)/N)*pi*(k-1)/N;
                dbas[d][index] = -stabilise*std::sqrt(2.0/(double)VGgeo[d])*std::sin(pi_inv_mni_dim*(value_type)i*((value_type)n+0.5))*pi_inv_mni_dim*i;
            }
        }
    }

};

template<typename parameter_type>
void bfnorm_mrqcof_zero_half(std::vector<parameter_type>& alpha,
                             std::vector<parameter_type>& beta)
{
    int m1 = beta.size();
    for (int x1=0;x1<m1;x1++)
    {
            for (int x2=0;x2<=x1;x2++)
                    alpha[m1*x1+x2] = 0.0;
            beta[x1]= 0.0;
    }

}

//[Alpha,Beta,Var,fw] = bfnorm_mrqcof(VG,VF,Affine,basX,basY,basZ,dbasX,dbasY,dbasZ,T,fwhm,VWG, VWF);
/*
INPUTS
T[3*nz*ny*nx + ni*4] - current transform
VF              - image to normalize
ni              - number of templates
vols2[ni]       - templates

nx              - number of basis functions in x
B0[dim1[0]*nx]  - basis functions in x
dB0[dim1[0]*nx] - derivatives of basis functions in x
ny              - number of basis functions in y
B1[dim1[1]*ny]  - basis functions in y
dB1[dim1[1]*ny] - derivatives of basis functions in y
nz              - number of basis functions in z
B2[dim1[2]*nz]  - basis functions in z
dB2[dim1[2]*nz] - derivatives of basis functions in z

M[4*4]          - transformation matrix
samp[3]         - frequency of sampling template.


OUTPUTS
alpha[(3*nz*ny*nx+ni*4)^2] - A'A
 beta[(3*nz*ny*nx+ni*4)]   - A'b
pss[1*1]                   - sum of squares difference
pnsamp[1*1]                - number of voxels sampled
ss_deriv[3]                - sum of squares of derivatives of residuals
*/
template<typename image_type,typename parameter_type,typename value_type>
void bfnorm_mrqcof(const image_type& VG,
            const image_type& VF,
            const bfnorm_mapping<value_type,3>& mapping,
            double fwhm,double fwhm2,
            std::vector<parameter_type>& alpha,
            std::vector<parameter_type>& beta,
            value_type& var,
            value_type& fw)
{
    const std::vector<parameter_type>& T = mapping.T;
    const std::vector<std::vector<parameter_type> >& base = mapping.bas;
    const std::vector<std::vector<parameter_type> >& dbase = mapping.dbas;

    value_type ss = 0.0,nsamp = 0.0;
    value_type ss_deriv[3]={0.0,0.0,0.0};
    int nx = mapping.k_base[0];
    int ny = mapping.k_base[1];
    int nz = mapping.k_base[2];
    int nxy = nx*ny;
    int nxyz = nxy*nz;
    int nx3 = nx*3;
    int nxy3 = nxy*3;
    int nxyz3 = nxyz*3;
    int edgeskip[3],samp[3];
    const std::vector<value_type>& B0 = base[0];
    const std::vector<value_type>& B1 = base[1];
    const std::vector<value_type>& B2 = base[2];
    const std::vector<value_type>& dB0 = dbase[0];
    const std::vector<value_type>& dB1 = dbase[1];
    const std::vector<value_type>& dB2 = dbase[2];

    const double *bz3[3], *by3[3], *bx3[3];

    bx3[0] = &dB0[0];	bx3[1] =  &B0[0];	bx3[2] =  &B0[0];
    by3[0] =  &B1[0];	by3[1] = &dB1[0];	by3[2] =  &B1[0];
    bz3[0] =  &B2[0];	bz3[1] =  &B2[0];	bz3[2] = &dB2[0];
    {


            /* Because of edge effects from the smoothing, ignore voxels that are too close */
            edgeskip[0]   = std::floor(fwhm); edgeskip[0] = ((edgeskip[0]<1) ? 0 : edgeskip[0]);
            edgeskip[1]   = std::floor(fwhm); edgeskip[1] = ((edgeskip[1]<1) ? 0 : edgeskip[1]);
            edgeskip[2]   = std::floor(fwhm); edgeskip[2] = ((edgeskip[2]<1) ? 0 : edgeskip[2]);


            /* sample about every fwhm/2 */
            samp[0]   = std::floor(fwhm/2.0); samp[0] = ((samp[0]<1) ? 1 : samp[0]);
            samp[1]   = std::floor(fwhm/2.0); samp[1] = ((samp[1]<1) ? 1 : samp[1]);
            samp[2]   = std::floor(fwhm/2.0); samp[2] = ((samp[2]<1) ? 1 : samp[2]);

            alpha.resize((nxyz3+4)*(nxyz3+4)); // plhs[0]
            beta.resize(nxyz3+4); // plhs[1]

    }


    int i1,i2, s0[3], x1,x2, y1,y2, z1,z2, m1, m2;
    value_type s2[3], tmp;
    value_type *ptr1, *ptr2;
    const value_type *scal,*ptr;
    value_type J[3][3];
    image::geometry<3> dim1(VG.geometry());

        /* rate of change of voxel with respect to change in parameters */
        std::vector<value_type> dvdt( nx3    + 4);

        /* Intermediate storage arrays */
        std::vector<value_type> Tz( nxy3 );
        std::vector<value_type> Ty( nx3 );
        std::vector<value_type> betax( nx3    + 4);
        std::vector<value_type> betaxy( nxy3 + 4);
        std::vector<value_type> alphax((nx3    + 4)*(nx3    + 4));
        std::vector<value_type> alphaxy((nxy3 + 4)*(nxy3 + 4));

        std::vector<std::vector<std::vector<double> > > Jz(3),Jy(3);

        for (i1=0; i1<3; i1++)
        {
            Jz[i1].resize(3);
            Jy[i1].resize(3);
                for(i2=0; i2<3; i2++)
                {
                        Jz[i1][i2].resize(nxy);
                        Jy[i1][i2].resize(nx);
                }
        }


        /* pointer to scales for each of the template images */
        scal = &T[nxyz3];

        /* only zero half the matrix */
        bfnorm_mrqcof_zero_half(alpha,beta);


        for(s0[2]=1; s0[2]<dim1[2]; s0[2]+=samp[2]) /* For each plane of the template images */
        {
                /* build up the deformation field (and derivatives) from it's seperable form */
                for(i1=0, ptr=&T[0]; i1<3; i1++, ptr += nxyz)
                        for(x1=0; x1<nxy; x1++)
                        {
                                /* intermediate step in computing nonlinear deformation field */
                                tmp = 0.0;
                                for(z1=0; z1<nz; z1++)
                                        tmp  += ptr[x1+z1*nxy] * B2[dim1[2]*z1+s0[2]];
                                Tz[nxy*i1 + x1] = tmp;

                                /* intermediate step in computing Jacobian of nonlinear deformation field */
                                for(i2=0; i2<3; i2++)
                                {
                                        tmp = 0;
                                        for(z1=0; z1<nz; z1++)
                                                tmp += ptr[x1+z1*nxy] * bz3[i2][dim1[2]*z1+s0[2]];
                                        Jz[i2][i1][x1] = tmp;
                                }
                        }

                /* only zero half the matrix */
                bfnorm_mrqcof_zero_half(alphaxy,betaxy);


                for(s0[1]=1; s0[1]<dim1[1]; s0[1]+=samp[1]) /* For each row of the template images plane */
                {
                        /* build up the deformation field (and derivatives) from it's seperable form */
                        for(i1=0, ptr=&Tz[0]; i1<3; i1++, ptr+=nxy)
                        {
                                for(x1=0; x1<nx; x1++)
                                {
                                        /* intermediate step in computing nonlinear deformation field */
                                        tmp = 0.0;
                                        for(y1=0; y1<ny; y1++)
                                                tmp  += ptr[x1+y1*nx] *  B1[dim1[1]*y1+s0[1]];
                                        Ty[nx*i1 + x1] = tmp;

                                        /* intermediate step in computing Jacobian of nonlinear deformation field */
                                        for(i2=0; i2<3; i2++)
                                        {
                                                tmp = 0;
                                                for(y1=0; y1<ny; y1++)
                                                        tmp += Jz[i2][i1][x1+y1*nx] * by3[i2][dim1[1]*y1+s0[1]];
                                                Jy[i2][i1][x1] = tmp;
                                        }
                                }
                        }

                        /* only zero half the matrix */
                        bfnorm_mrqcof_zero_half(alphax,betax);

                        for(s0[0]=1; s0[0]<dim1[0]; s0[0]+=samp[0]) /* For each pixel in the row */
                        {
                                double trans[3];

                                /* nonlinear deformation of the template space, followed by the affine transform */
                                for(i1=0, ptr = &Ty[0]; i1<3; i1++, ptr += nx)
                                {
                                        /* compute nonlinear deformation field */
                                        tmp = 0.0;
                                        for(x1=0; x1<nx; x1++)
                                                tmp  += ptr[x1] * B0[dim1[0]*x1+s0[0]];
                                        trans[i1] = tmp + s0[i1];

                                        /* compute Jacobian of nonlinear deformation field */
                                        for(i2=0; i2<3; i2++)
                                        {
                                                if (i1 == i2) tmp = 1.0; else tmp = 0;
                                                for(x1=0; x1<nx; x1++)
                                                        tmp += Jy[i2][i1][x1] * bx3[i2][dim1[0]*x1+s0[0]];
                                                J[i2][i1] = tmp;
                                        }
                                }

                                s2[0] = trans[0];
                                s2[1] = trans[1];
                                s2[2] = trans[2];

                                /* is the transformed position in range? */
                                if (	s2[0]>=1+edgeskip[0] && s2[0]< VF.width()-edgeskip[0] &&
                                        s2[1]>=1+edgeskip[1] && s2[1]< VF.height()-edgeskip[1] &&
                                        s2[2]>=1+edgeskip[2] && s2[2]< VF.depth()-edgeskip[2] )
                                {
                                        double f, df[3], dv, dvds0[3];
                                        double wtf, wtg, wt;
                                        double s0d[3];
                                        s0d[0]=s0[0];s0d[1]=s0[1];s0d[2]=s0[2];

                                        f = resample_d(VF,df[0],df[1],df[2],s2[0],s2[1],s2[2]);

                                        wtg = 1.0;
                                        wtf = 1.0;

                                        if (wtf && wtg) wt = sqrt(1.0 /(1.0/wtf + 1.0/wtg));
                                        else wt = 0.0;

                                        /* nonlinear transform the gradients to the same space as the template */
                                        image::vector_rotation(df,dvds0,&(J[0][0]),image::vdim<3>());

                                        dv = f;
                                        {
                                                double g, dg[3], tmp;
                                                g = resample_d(VG,dg[0],dg[1],dg[2],s0d[0],s0d[1],s0d[2]);

                                                /* linear combination of image and image modulated by constant
                                                   gradients in x, y and z */
                                                dvdt[nx3] = wt*g;
                                                dvdt[1+nx3] = dvdt[nx3]*s2[0];
                                                dvdt[2+nx3] = dvdt[nx3]*s2[1];
                                                dvdt[3+nx3] = dvdt[nx3]*s2[2];

                                                tmp = scal[0] + s2[0]*scal[1] + s2[1]*scal[2] + s2[2]*scal[3];

                                                dv       -= tmp*g;
                                                dvds0[0] -= tmp*dg[0];
                                                dvds0[1] -= tmp*dg[1];
                                                dvds0[2] -= tmp*dg[2];
                                        }

                                        for(i1=0; i1<3; i1++)
                                        {
                                                double tmp = -wt*df[i1];
                                                for(x1=0; x1<nx; x1++)
                                                        dvdt[i1*nx+x1] = tmp * B0[dim1[0]*x1+s0[0]];
                                        }

                                        /* cf Numerical Recipies "mrqcof.c" routine */
                                        m1 = nx3+4;
                                        for(x1=0; x1<m1; x1++)
                                        {
                                                for (x2=0;x2<=x1;x2++)
                                                        alphax[m1*x1+x2] += dvdt[x1]*dvdt[x2];
                                                betax[x1] += dvdt[x1]*dv*wt;
                                        }

                                        /* sum of squares */
                                        wt          *= wt;
                                        nsamp       += wt;
                                        ss          += wt*dv*dv;
                                        ss_deriv[0] += wt*dvds0[0]*dvds0[0];
                                        ss_deriv[1] += wt*dvds0[1]*dvds0[1];
                                        ss_deriv[2] += wt*dvds0[2]*dvds0[2];
                                }
                        }

                        m1 = nxy3+4;
                        m2 = nx3+4;

                        /* Kronecker tensor products */
                        for(y1=0; y1<ny; y1++)
                        {
                                double wt1 = B1[dim1[1]*y1+s0[1]];

                                for(i1=0; i1<3; i1++)	/* loop over deformations in x, y and z */
                                {
                                        /* spatial-spatial covariances */
                                        for(i2=0; i2<=i1; i2++)	/* symmetric matrixes - so only work on half */
                                        {
                                                for(y2=0; y2<=y1; y2++)
                                                {
                                                        /* Kronecker tensor products with B1'*B1 */
                                                        double wt2 = wt1 * B1[dim1[1]*y2+s0[1]];

                                                        ptr1 = &alphaxy[nx*(m1*(ny*i1 + y1) + ny*i2 + y2)];
                                                        ptr2 = &alphax[nx*(m2*i1 + i2)];

                                                        for(x1=0; x1<nx; x1++)
                                                        {
                                                                for (x2=0;x2<=x1;x2++)
                                                                        ptr1[m1*x1+x2] += wt2 * ptr2[m2*x1+x2];
                                                        }
                                                }
                                        }

                                        /* spatial-intensity covariances */
                                        ptr1 = &alphaxy[nx*(m1*ny*3 + ny*i1 + y1)];
                                        ptr2 = &alphax[nx*(m2*3 + i1)];
                                        for(x1=0; x1<4; x1++)
                                        {
                                                for (x2=0;x2<nx;x2++)
                                                        ptr1[m1*x1+x2] += wt1 * ptr2[m2*x1+x2];
                                        }

                                        /* spatial component of beta */
                                        for(x1=0; x1<nx; x1++)
                                                betaxy[x1+nx*(ny*i1 + y1)] += wt1 * betax[x1 + nx*i1];
                                }
                        }
                        ptr1 = &alphaxy[nx*(m1*ny*3 + ny*3)];
                        ptr2 = &alphax[nx*(m2*3 + 3)];
                        for(x1=0; x1<4; x1++)
                        {
                                /* intensity-intensity covariances  */
                                for (x2=0; x2<=x1; x2++)
                                        ptr1[m1*x1 + x2] += ptr2[m2*x1 + x2];

                                /* intensity component of beta */
                                betaxy[nxy3 + x1] += betax[nx3 + x1];
                        }
                }

                m1 = nxyz3+4;
                m2 = nxy3+4;

                /* Kronecker tensor products */
                for(z1=0; z1<nz; z1++)
                {
                        double wt1 = B2[dim1[2]*z1+s0[2]];

                        for(i1=0; i1<3; i1++)	/* loop over deformations in x, y and z */
                        {
                                /* spatial-spatial covariances */
                                for(i2=0; i2<=i1; i2++)	/* symmetric matrixes - so only work on half */
                                {
                                        for(z2=0; z2<=z1; z2++)
                                        {
                                                /* Kronecker tensor products with B2'*B2 */
                                                double wt2 = wt1 * B2[dim1[2]*z2+s0[2]];

                                                ptr1 = &alpha[nxy*(m1*(nz*i1 + z1) + nz*i2 + z2)];
                                                ptr2 = &alphaxy[nxy*(m2*i1 + i2)];
                                                for(y1=0; y1<nxy; y1++)
                                                {
                                                        for (y2=0;y2<=y1;y2++)
                                                                ptr1[m1*y1+y2] += wt2 * ptr2[m2*y1+y2];
                                                }
                                        }
                                }

                                /* spatial-intensity covariances */
                                ptr1 = &alpha[nxy*(m1*nz*3 + nz*i1 + z1)];
                                ptr2 = &alphaxy[nxy*(m2*3 + i1)];
                                for(y1=0; y1<4; y1++)
                                {
                                        for (y2=0;y2<nxy;y2++)
                                                ptr1[m1*y1+y2] += wt1 * ptr2[m2*y1+y2];
                                }

                                /* spatial component of beta */
                                for(y1=0; y1<nxy; y1++)
                                        beta[y1 + nxy*(nz*i1 + z1)] += wt1 * betaxy[y1 + nxy*i1];
                        }
                }

                ptr1 = &alpha[nxy*(m1*nz*3 + nz*3)];
                ptr2 = &alphaxy[nxy*(m2*3 + 3)];
                for(y1=0; y1<4; y1++)
                {
                        /* intensity-intensity covariances */
                        for(y2=0;y2<=y1;y2++)
                                ptr1[m1*y1 + y2] += ptr2[m2*y1 + y2];

                        /* intensity component of beta */
                        beta[nxyz3 + y1] += betaxy[nxy3 + y1];
                }
        }


        /* Fill in the symmetric bits
           - OK I know some bits are done more than once. */

        m1 = nxyz3+4;
        for(i1=0; i1<3; i1++)
        {
                double *ptrz, *ptry, *ptrx;
                for(i2=0; i2<=i1; i2++)
                {
                        ptrz = &alpha[nxyz*(m1*i1 + i2)];
                        for(z1=0; z1<nz; z1++)
                                for(z2=0; z2<=z1; z2++)
                                {
                                        ptry = ptrz + nxy*(m1*z1 + z2);
                                        for(y1=0; y1<ny; y1++)
                                                for (y2=0;y2<=y1;y2++)
                                                {
                                                        ptrx = ptry + nx*(m1*y1 + y2);
                                                        for(x1=0; x1<nx; x1++)
                                                                for(x2=0; x2<x1; x2++)
                                                                        ptrx[m1*x2+x1] = ptrx[m1*x1+x2];
                                                }
                                        for(x1=0; x1<nxy; x1++)
                                                for (x2=0; x2<x1; x2++)
                                                        ptry[m1*x2+x1] = ptry[m1*x1+x2];
                                }
                        for(x1=0; x1<nxyz; x1++)
                                for (x2=0; x2<x1; x2++)
                                        ptrz[m1*x2+x1] = ptrz[m1*x1+x2];
                }
        }
        for(x1=0; x1<nxyz3+4; x1++)
                for (x2=0; x2<x1; x2++)
                        alpha[m1*x2+x1] = alpha[m1*x1+x2];

        //

        fw = ((1.0/std::sqrt(2.0*ss_deriv[0]/ss))*sqrt(8.0*std::log(2.0)) +
                 (1.0/std::sqrt(2.0*ss_deriv[1]/ss))*sqrt(8.0*std::log(2.0)) +
                 (1.0/std::sqrt(2.0*ss_deriv[2]/ss))*sqrt(8.0*std::log(2.0)))/3.0;


        if (fw<fwhm2)
                fwhm2 = fw;
        if (fwhm2<fwhm)
                fwhm2 = fwhm;

        ss /= (std::min(samp[0]/(fwhm2*1.0645),1.0) *
              std::min(samp[1]/(fwhm2*1.0645),1.0) *
              std::min(samp[2]/(fwhm2*1.0645),1.0)) * (nsamp - (nxyz3 + 4));

        var = ss;
        image::divide_constant(alpha.begin(),alpha.end(), ss);
        image::divide_constant(beta.begin(),beta.end(), ss);

}




template<typename ImageType,typename value_type>
void bfnorm(const ImageType& VG,const ImageType& VF,bfnorm_mapping<value_type>& mapping,
             value_type sample_rate = (value_type)8.0,int iteration = 16)
{
    static const int dim = ImageType::dimension;
    value_type fwhm2= 30;
    value_type stabilise = 8,reg = 1.0;
    int s1 = 3*mapping.k_base.size();
    std::vector<value_type> IC0(3*mapping.k_base.size()+4);
    {
        std::vector<std::vector<value_type> > kxyz(3);
        for(int d = 0;d < dim;++d)
        {
            kxyz[d].resize(mapping.k_base[d]);
            for(int i = 0;i < kxyz[d].size();++i)
            {
                kxyz[d][i] = 3.14159265358979323846*(value_type)i/mapping.VGgeo[d];
                kxyz[d][i] *= kxyz[d][i];
            }
        }
        int ICO_ = mapping.k_base.size();
        value_type IC0_co = reg*std::pow(stabilise,6);
        for(image::pixel_index<dim> pos;pos.valid(mapping.k_base);pos.next(mapping.k_base))
        {
            int m = pos[0];
            int j = pos[1];
            int i = pos[2];
            int index = pos.index();
            IC0[index] = kxyz[2][i]*kxyz[2][i]+kxyz[1][j]*kxyz[1][j]+kxyz[0][m]*kxyz[0][m]+
                         2*kxyz[0][m]*kxyz[1][j]+2*kxyz[0][m]*kxyz[2][i]+2*kxyz[1][j]*kxyz[2][i];
            IC0[index] *= IC0_co;
            IC0[index+ICO_] = IC0[index];
            IC0[index+ICO_+ICO_] = IC0[index];
        }
    }

    mapping.T.resize(3*mapping.k_base.size()+4);
    std::fill(mapping.T.begin(),mapping.T.end(),0);
    mapping.T[s1] = 1;
    std::vector<value_type> alpha,beta;
    value_type var,fw,pvar = std::numeric_limits<value_type>::max();

    for(int iter = 0;iter < iteration;++iter)
    {
        bfnorm_mrqcof(VG,VF,mapping,sample_rate,fwhm2,alpha,beta,var,fw);
        {
            // beta = beta + alpha*T;
            std::vector<value_type> alphaT(mapping.T.size());
            image::matrix::vector_product(alpha.begin(),mapping.T.begin(),alphaT.begin(),image::dyndim(mapping.T.size(),mapping.T.size()));
            image::add(beta.begin(),beta.end(),alphaT.begin());
        }

        //Alpha + IC0*scal
        if(var > pvar)
        {
            value_type scal = pvar/var;
            var = pvar;
            for(int i = 0,j = 0;i < alpha.size();i+=mapping.T.size()+1,++j)
                alpha[i] += IC0[j]*scal;
        }
        else
            for(int i = 0,j = 0;i < alpha.size();i+=mapping.T.size()+1,++j)
                alpha[i] += IC0[j];

        // solve T = (Alpha + IC0*scal)\(Alpha*T + Beta);
        {
            std::vector<value_type> piv(mapping.T.size());
            image::matrix::ll_decomposition(&*alpha.begin(),&*piv.begin(),image::dyndim(mapping.T.size(),mapping.T.size()));
            image::matrix::ll_solve(&*alpha.begin(),&*piv.begin(),&*beta.begin(),&*mapping.T.begin(),image::dyndim(mapping.T.size(),mapping.T.size()));
        }
        fwhm2 = std::min(fw,fwhm2);
        //std::cout << "FWHM = " << fw << " Var = " << var <<std::endl;
    }
}

template<typename value_type,typename rhs_type1,typename rhs_type2>
bool bfnorm_warp_coordinate(const bfnorm_mapping<value_type>& mapping,const rhs_type1& from,rhs_type2& to)
{
    to = from;
    if(!mapping.VGgeo.is_valid(from))
        return false;
    int nx = mapping.k_base[0];
    int ny = mapping.k_base[1];
    int nz = mapping.k_base[2];
    int nyz =ny*nz;
    int nxyz = mapping.k_base.size();

    const std::vector<value_type>& T = mapping.T;

    image::dyndim dyz_x(nyz,nx),dz_y(nz,ny),dx_1(nx,1),dy_1(ny,1);
    std::vector<value_type> bx_(nx),by_(ny),bz_(nz),temp_(nyz),temp2_(nz);
    value_type *bx = &bx_[0];
    value_type *by = &by_[0];
    value_type *bz = &bz_[0];
    value_type *temp = &temp_[0];
    value_type *temp2 = &temp2_[0];

    for(unsigned char dim = 0;dim < 3;++dim)
    {
    for(unsigned int k = 0,index = from[0];k < nx;++k,index += mapping.VGgeo[0])
        bx[k] = mapping.bas[0][index];
    for(unsigned int k = 0,index = from[1];k < ny;++k,index += mapping.VGgeo[1])
        by[k] = mapping.bas[1][index];
    for(unsigned int k = 0,index = from[2];k < nz;++k,index += mapping.VGgeo[2])
        bz[k] = mapping.bas[2][index];
    }

    image::matrix::product(T.begin(),bx,temp,dyz_x,dx_1);
    image::matrix::product(temp,by,temp2,dz_y,dy_1);
    to[0] += math::vector_op_dot(bz,bz+nz,temp2);

    image::matrix::product(T.begin()+nxyz,bx,temp,dyz_x,dx_1);
    image::matrix::product(temp,by,temp2,dz_y,dy_1);
    to[1] += math::vector_op_dot(bz,bz+nz,temp2);

    image::matrix::product(T.begin()+(nxyz << 1),bx,temp,dyz_x,dx_1);
    image::matrix::product(temp,by,temp2,dz_y,dy_1);
    to[2] += math::vector_op_dot(bz,bz+nz,temp2);
    return true;
}

template<typename value_type,typename from_type,typename matrix_type>
void bfnorm_get_jacobian(const bfnorm_mapping<value_type>& mapping,const from_type& from,matrix_type Jbet)
{
    int nx = mapping.k_base[0];
    int ny = mapping.k_base[1];
    int nz = mapping.k_base[2];
    int nyz =ny*nz;
    int nxyz = mapping.k_base.size();

    const std::vector<value_type>& T = mapping.T;

    std::vector<double> bx_(nx),by_(ny),bz_(nz),dbx_(nx),dby_(ny),dbz_(nz),temp_(nyz),temp2_(nz);
    double *bx = &bx_[0];
    double *by = &by_[0];
    double *bz = &bz_[0];
    double *dbx = &dbx_[0];
    double *dby = &dby_[0];
    double *dbz = &dbz_[0];
    double *temp = &temp_[0];
    double *temp2 = &temp2_[0];

    for(unsigned int k = 0,index = from[0];k < nx;++k,index += mapping.VGgeo[0])
    {
        bx[k] = mapping.bas[0][index];
        dbx[k] = mapping.dbas[0][index];
    }
    for(unsigned int k = 0,index = from[1];k < ny;++k,index += mapping.VGgeo[1])
    {
        by[k] = mapping.bas[1][index];
        dby[k] = mapping.dbas[1][index];
    }
    for(unsigned int k = 0,index = from[2];k < nz;++k,index += mapping.VGgeo[2])
    {
        bz[k] = mapping.bas[2][index];
        dbz[k] = mapping.dbas[2][index];
    }
    image::dyndim dyz_x(nyz,nx),dz_y(nz,ny),dx_1(nx,1),dy_1(ny,1);


    // f(x)/dx
    image::matrix::product(T.begin(),dbx,temp,dyz_x,dx_1);
    image::matrix::product(temp,by,temp2,dz_y,dy_1);
    Jbet[0] = 1 + math::vector_op_dot(bz,bz+nz,temp2);
    // f(x)/dy
    image::matrix::product(T.begin(),bx,temp,dyz_x,dx_1);
    image::matrix::product(temp,dby,temp2,dz_y,dy_1);
    Jbet[1] = math::vector_op_dot(bz,bz+nz,temp2);
    // f(x)/dz
    image::matrix::product(T.begin(),bx,temp,dyz_x,dx_1);
    image::matrix::product(temp,by,temp2,dz_y,dy_1);
    Jbet[2] = math::vector_op_dot(dbz,dbz+nz,temp2);

    // f(y)/dx
    image::matrix::product(T.begin()+nxyz,dbx,temp,dyz_x,dx_1);
    image::matrix::product(temp,by,temp2,dz_y,dy_1);
    Jbet[3] = math::vector_op_dot(bz,bz+nz,temp2);
    // f(y)/dy
    image::matrix::product(T.begin()+nxyz,bx,temp,dyz_x,dx_1);
    image::matrix::product(temp,dby,temp2,dz_y,dy_1);
    Jbet[4] = 1 + math::vector_op_dot(bz,bz+nz,temp2);
    // f(y)/dz
    image::matrix::product(T.begin()+nxyz,bx,temp,dyz_x,dx_1);
    image::matrix::product(temp,by,temp2,dz_y,dy_1);
    Jbet[5] = math::vector_op_dot(dbz,dbz+nz,temp2);

    // f(z)/dx
    image::matrix::product(T.begin()+(nxyz << 1),dbx,temp,dyz_x,dx_1);
    image::matrix::product(temp,by,temp2,dz_y,dy_1);
    Jbet[6] = math::vector_op_dot(bz,bz+nz,temp2);
    // f(z)/dy
    image::matrix::product(T.begin()+(nxyz << 1),bx,temp,dyz_x,dx_1);
    image::matrix::product(temp,dby,temp2,dz_y,dy_1);
    Jbet[7] = math::vector_op_dot(bz,bz+nz,temp2);
    // f(z)/dz
    image::matrix::product(T.begin()+(nxyz << 1),bx,temp,dyz_x,dx_1);
    image::matrix::product(temp,by,temp2,dz_y,dy_1);
    Jbet[8] = 1 + math::vector_op_dot(dbz,dbz+nz,temp2);

    //image::matrix::product(affine_rotation,Jbet,M,math::dim<3,3>(),math::dim<3,3>());
}


template<typename value_type,typename image_type1,typename image_type2>
void bfnorm_warp_image(const bfnorm_mapping<value_type>& mapping,const image_type1& I,image_type2& out)
{
    out.resize(VGgeo);
    for(image::pixel_index<3> index;VGgeo.is_valid(index);index.next(VGgeo))
    {
        image::vector<3,double> pos;
        bfnorm_warp_coordinate(mapping,index,pos);
        image::interpolation<image::linear_weighting,3> trilinear_interpolation;
        trilinear_interpolation.estimate(I,pos,out[index.index()]);
    }
}


}// reg
}// image

#endif
