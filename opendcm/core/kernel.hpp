/*
    openDCM, dimensional constraint manager
    Copyright (C) 2012  Stefan Troeger <stefantroeger@gmx.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef DCM_KERNEL_H
#define DCM_KERNEL_H

#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/Geometry>

#include <iostream>
#include <../FreeCAD/src/Base/Console.h>

#include <boost/math/special_functions/fpclassify.hpp>


namespace dcm {

namespace E = Eigen;

enum ParameterType {
    Rotation,
    Translation,
    Anything
};

template<typename Kernel>
struct Dogleg {

    typedef typename Kernel::number_type number_type;
    number_type tolg, tolx, tolf;

    Dogleg() : tolg(1e-80), tolx(1e-10), tolf(1e-10) {};

    template <typename Derived, typename Derived2, typename Derived3, typename Derived4>
    int calculateStep(const Eigen::MatrixBase<Derived>& g, const Eigen::MatrixBase<Derived3>& jacobi,
                      const Eigen::MatrixBase<Derived4>& residual, Eigen::MatrixBase<Derived2>& h_dl,
                      const double delta) {

         std::stringstream stream;
         //stream<</*"g: "<<std::endl<<g<<std::endl<<*/"jacobi: "<<std::endl<<jacobi<<std::endl;
         //stream<<"residual: "<<std::endl<<residual<<std::endl;


        // get the steepest descent stepsize and direction
        const double alpha(g.squaredNorm()/(jacobi*g).squaredNorm());
        const typename Kernel::Vector h_sd  = -g;

        // get the gauss-newton step
	bool force_sd;
        //const typename Kernel::Vector h_gn = jacobi.fullPivLu().solve(-residual);
	Eigen::JacobiSVD<typename Kernel::Matrix> svd(jacobi, Eigen::ComputeThinU | Eigen::ComputeThinV);
	const typename Kernel::Vector h_gn = svd.solve(-residual);
	if( boost::math::isfinite(h_gn.norm()) ) {
	  //stream<<"normal h_gn"<<std::endl;
	  force_sd = false;
	}
	else {
	  stream<<"force steepest descend"<<std::endl;
	  stream<<"jacobi"<<std::endl<<jacobi<<std::endl;
	  force_sd = true; 
	}
	//stream<<"h_gn:"<<std::endl<<h_gn<<std::endl;

        // compute the dogleg step
        if( !force_sd && (h_gn.norm() <= delta)) {
            // std::cout<<"Gauss Newton"<<std::endl;
            h_dl = h_gn;
            //if(h_dl.norm() <= tolx*(tolx + sys.Parameter.norm())) {
            //    return 5;
            //}
        } else if( force_sd || ((alpha*h_sd).norm() >= delta)) {
            // std::cout<<"Steepest descent"<<std::endl;
            //h_dl = alpha*h_sd;
            h_dl = (delta/(h_sd.norm()))*h_sd;
	    //stream<<"h_dl steepest: "<<std::endl<<h_dl<<std::endl;
            //die theorie zu dogleg sagt: h_dl = (delta/(h_sd.norm()))*h_sd;
            //wir gehen aber den klassichen steepest descent weg
        } else {
            // std::cout<<"Dogleg"<<std::endl;
            //compute beta
            number_type beta = 0;
            typename Kernel::Vector a = alpha*h_sd;
            typename Kernel::Vector b = h_gn;
           number_type c = a.transpose()*(b-a);
            number_type bas = (b-a).squaredNorm(), as = a.squaredNorm();
            if(c<0) {
                beta = -c+std::sqrt(std::pow(c,2)+bas*(std::pow(delta,2)-as));
                beta /= bas;
            } else {
                beta = std::pow(delta,2)-as;
                beta /= c+std::sqrt(std::pow(c,2) + bas*(std::pow(delta,2)-as));
            };

            // and update h_dl and dL with beta
            h_dl = alpha*h_sd + beta*(b-a);
        }
        // stream<<"jacobi*h_dl"<<std::endl<<(jacobi*h_dl)<<std::endl<<std::endl;
        // stream<<"h_dl:"<<std::endl<<h_dl<<std::endl<<std::endl;
         Base::Console().Message("%s", stream.str().c_str());
        return 0;
    };

    bool solve(typename Kernel::MappedEquationSystem& sys) {
        std::cout<<"start solving"<<std::endl;

        if(!sys.isValid()) return false;

        int npt = sys.m_params+sys.m_trans_params;
        int npr = sys.m_rot_params;
	bool translate = true;

        //Base::Console().Message("\nparams: %d, rot_params: %d, trans_params: %d\n", sys.m_params, sys.m_rot_params, sys.m_trans_params);
        typename Kernel::Vector h_dlt(npt), h_dlr(npr),
                 F_old(sys.m_eqns), g(sys.m_eqns);
        typename Kernel::Matrix J_old(sys.m_eqns, npt+npr);

        sys.recalculate();

        std::stringstream stream;
        stream<<"start jacobi: "<<std::endl<<sys.Jacobi<<std::endl<<std::endl;
        Base::Console().Message("%s", stream.str().c_str());
	stream.str(std::string());

        number_type err = sys.Residual.norm();

        F_old = sys.Residual;
        J_old = sys.Jacobi;

        g = sys.Jacobi.transpose()*(sys.Residual);
        if((npt != 0) && (g.head(npt).norm() == 0)) translate=false;   //means that translations are existend but have no influence

        // get the infinity norm fx_inf and g_inf
        number_type g_inf = g.template lpNorm<E::Infinity>();
        number_type fx_inf = sys.Residual.template lpNorm<E::Infinity>();

        int maxIterNumber = 1000;//MaxIterations * xsize;
        number_type diverging_lim = 1e6*err + 1e12;

        number_type delta_t=10, delta_t_p=10, delta_r=0.1;
        number_type nu_t=2., nu_r=2.;
        int iter=0, stop=0, reduce=0;

/*        std::stringstream stream;
        stream<<"jacobi:"<<std::endl<<sys.Jacobi<<std::endl<<std::endl;
	stream<<"parameter:"<<std::endl<<sys.Parameter.transpose()<<std::endl<<std::endl;
        stream<<std::fixed<<std::setprecision(5)<<"delta_t: "<<delta_t<<",   delta_r: " << delta_r;
        stream<<", initial residual:"<<sys.Residual.transpose()<<std::endl;
        Base::Console().Message("%s", stream.str().c_str());
*/
        while(!stop) {

            // check if finished
            if(fx_inf <= tolf)  // Success
                stop = 1;
            else if(g_inf <= tolg)
                stop = 2;
            else if(delta_t <= tolx && delta_r <= tolx)
                stop = 3;
            else if(iter >= maxIterNumber)
                stop = 4;
            else if(err > diverging_lim || err != err) {  // check for diverging and NaN
                stop = 6;
            }

            // see if we are already finished
            if(stop)
                break;

            number_type dF_t=0, dL_t=0;
            number_type rho_t, err_new_t;

            if(npt && translate) {
                //get the update step
                calculateStep(g.head(npt), sys.Jacobi.block(0, 0, sys.m_eqns, npt),
                              sys.Residual, h_dlt, delta_t);

                // calculate the linear model
                dL_t = 0.5*sys.Residual.norm() - 0.5*(sys.Residual + sys.Jacobi.block(0, 0, sys.m_eqns, npt)*h_dlt).norm();

                std::stringstream stream;
                //stream<<"residual:"<<std::endl<<sys.Residual<<std::endl<<std::endl;
                //stream<<"residual + jacobi*h_dl"<<std::endl<<(sys.Residual + sys.Jacobi.block(0, 0, sys.m_eqns, npt)*h_dlt)<<std::endl<<std::endl;
                //Base::Console().Message("%s", stream.str().c_str());

                // get the new values
                sys.Parameter.head(npt) += h_dlt;
                sys.recalculate();

                //calculate the translation update ratio
                err_new_t = sys.Residual.norm();
                dF_t = err - err_new_t;
                rho_t = dF_t/dL_t;

                if(dF_t<=0 && dL_t<=0) rho_t = -1;
                // update delta
                if(rho_t>0.75) {
                    delta_t = std::max(delta_t,3*h_dlt.norm());
                    nu_t = 2;
                } else if(rho_t < 0.25) {
                    delta_t = delta_t/nu_t;
                    nu_t = 2*nu_t;
                }

                if(dF_t > 0 && dL_t > 0) {

                    F_old = sys.Residual;
                    J_old = sys.Jacobi;

                    err = err_new_t;

                    g = sys.Jacobi.transpose()*(sys.Residual);

                    // get infinity norms
                    g_inf = g.template lpNorm<E::Infinity>();
                    fx_inf = sys.Residual.template lpNorm<E::Infinity>();

                } else {
                    // std::cout<<"Step Rejected"<<std::endl;
                    sys.Residual = F_old;
                    sys.Jacobi = J_old;
                    sys.Parameter.head(npt) -= h_dlt;
                }
            }


            number_type dF_r=0, dL_r=0;
            number_type rho_r, err_new_r;
            if(npr) {

                //get the update step
                calculateStep(g.tail(npr), sys.Jacobi.block(0,npt, sys.m_eqns, npr),
                              sys.Residual, h_dlr, delta_r);

                //calculate linear model
                dL_r = 0.5*sys.Residual.norm() - 0.5*(sys.Residual
                                                      + sys.Jacobi.block(0,npt, sys.m_eqns, npr)*h_dlr).norm();

                sys.Parameter.tail(npr) += h_dlr;
                sys.recalculate();

                //calculate the rotation update ratio
                err_new_r = sys.Residual.norm();
                dF_r = err - err_new_r;
                number_type rho_r = dF_r/dL_r;

                if(dF_r<=0 && dL_r<=0) rho_t = -1;
                // update delta
                if(rho_r>0.75) {
                    delta_r = std::max(delta_r,3*h_dlr.norm());
                    nu_r = 2;
                } else if(rho_r < 0.25) {
                    delta_r = delta_r/nu_r;
                    nu_r = 2*nu_r;
                }

                if(dF_r > 0 && dL_r > 0) {

                    F_old = sys.Residual;
                    J_old = sys.Jacobi;

                    err = err_new_r;

                    g = sys.Jacobi.transpose()*(sys.Residual);

                    // get infinity norms
                    g_inf = g.template lpNorm<E::Infinity>();
                    fx_inf = sys.Residual.template lpNorm<E::Infinity>();
                } else {
                    // std::cout<<"Step Rejected"<<std::endl;
                    sys.Residual = F_old;
                    sys.Jacobi = J_old;
                    sys.Parameter.tail(npr) -= h_dlr;
                }
            }
            delta_t_p = delta_t;


           // std::stringstream stream;
           // stream<<std::fixed<<std::setprecision(5)<<"delta_t: "<<delta_t<<",   delta_r: " << delta_r;
           // stream<<",  parameter:"<<sys.Parameter.transpose()<<std::endl;
           // Base::Console().Message("%s", stream.str().c_str());
            // std::cout<<"Delta: "<<delta<<std::endl<<std::endl;
            // count this iteration and start again
            iter++;
        }
        stream<<"end jacobi: "<<std::endl<<sys.Jacobi<<std::endl<<std::endl;
        Base::Console().Message("%s", stream.str().c_str());
        // std::cout<<"Iterations used: "<<iter<<std::endl<<std::endl;
        Base::Console().Message("residual: %e, reason: %d, iterations: %d\n", err, stop, iter);
        //std::cout<<"DONE solving"<<std::endl;
        if(stop == 1) return true;
        return false; //TODO:throw
    }
};

template<typename Scalar, template<class> class Solver = Dogleg>
struct Kernel {

    //basics
    typedef Scalar number_type;

    //Linear algebra types
    typedef E::Matrix<Scalar, 3, 1> Vector3;
    typedef E::Matrix<Scalar, 1, 3> CVector3;
    typedef E::Matrix<Scalar, 3, 3> Matrix3;
    typedef E::Matrix<Scalar, E::Dynamic, 1> Vector;
    typedef E::Matrix<Scalar, 1, E::Dynamic> CVector;
    typedef E::Matrix<Scalar, E::Dynamic, E::Dynamic> Matrix;

    //mapped types
    typedef E::Stride<E::Dynamic, E::Dynamic> DynStride;
    typedef E::Map< Vector3 > Vector3Map;
    typedef E::Map< CVector3> CVector3Map;
    typedef E::Map< Matrix3 > Matrix3Map;
    typedef E::Map< Vector, 0, DynStride > VectorMap;
    typedef E::Map< CVector, 0, DynStride > CVectorMap;
    typedef E::Map< Matrix, 0, DynStride > MatrixMap;

    //Special types
    typedef E::Quaternion<Scalar>   Quaternion;
    typedef E::Matrix<Scalar, 3, 9> Matrix39;
    typedef E::Map< Matrix39 >      Matrix39Map;
    typedef E::Block<Matrix>	    MatrixBlock;

    struct MappedEquationSystem {

        Matrix Jacobi;
        Vector Parameter;
        Vector Residual;
        int m_params, m_rot_params, m_trans_params, m_eqns; //total amount
        int m_rot_offset, m_trans_offset, m_param_offset, m_eqn_offset;   //current positions while creation

        MappedEquationSystem(int params, int rotparams, int transparams, int equations)
            : Jacobi(equations, params+rotparams+transparams),
              Parameter(params+rotparams+transparams), Residual(equations),
              m_params(params), m_rot_params(rotparams), m_trans_params(transparams), m_eqns(equations) {
            m_rot_offset = params+transparams;
            m_trans_offset = params;
            m_param_offset = 0;
            m_eqn_offset = 0;
	    
	    Jacobi.setZero(); //important as some places are never written
        };

        int setParameterMap(ParameterType t, int number, VectorMap& map) {
            if(t==Anything) {
                new(&map) VectorMap(&Parameter(m_param_offset), number, DynStride(1,1));
                m_param_offset += number;
                return m_param_offset-number;
            } else if(t==Rotation) {
                new(&map) VectorMap(&Parameter(m_rot_offset), number, DynStride(1,1));
                m_rot_offset += number;
                return m_rot_offset-number;
            } else if(t==Translation) {
                new(&map) VectorMap(&Parameter(m_trans_offset), number, DynStride(1,1));
                m_trans_offset += number;
                return m_trans_offset-number;
            }
        };
        int setParameterMap(ParameterType t, Vector3Map& map) {
            if(t==Anything) {
                new(&map) Vector3Map(&Parameter(m_param_offset));
                m_param_offset += 3;
                return m_param_offset-3;
            } else if(t==Rotation) {
                new(&map) Vector3Map(&Parameter(m_rot_offset));
                m_rot_offset += 3;
                return m_rot_offset-3;
            } else if(t==Translation) {
                new(&map) Vector3Map(&Parameter(m_trans_offset));
                m_trans_offset += 3;
                return m_trans_offset-3;
            }
        };
        int setResidualMap(VectorMap& map) {
            new(&map) VectorMap(&Residual(m_eqn_offset), 1, DynStride(1,1));
	    Base::Console().Message("New residual added\n");
            return m_eqn_offset++;
        };
        void setJacobiMap(int eqn, int offset, int number, CVectorMap& map) {
            new(&map) CVectorMap(&Jacobi(eqn, offset), number, DynStride(0,m_eqns));
        };
        void setJacobiMap(int eqn, int offset, int number, VectorMap& map) {
            new(&map) VectorMap(&Jacobi(eqn, offset), number, DynStride(0,m_eqns));
        };

        bool isValid() {
            if(!(m_params+m_trans_params+m_rot_params) || !m_eqns) return false;

            return true;
        };

        virtual void recalculate() = 0;

    };

    Kernel()  {};

    template <typename DerivedA,typename DerivedB>
    static bool isSame(const E::MatrixBase<DerivedA>& p1,const E::MatrixBase<DerivedB>& p2) {
        return ((p1-p2).squaredNorm() < 0.001);
    }
    static bool isSame(number_type t1, number_type t2) {
        return ((t1-t2) < 0.001);
    }
    template <typename DerivedA,typename DerivedB>
    static bool isOpposite(const E::MatrixBase<DerivedA>& p1,const E::MatrixBase<DerivedB>& p2) {
        return ((p1+p2).squaredNorm() < 0.001);
    }

    static bool solve(MappedEquationSystem& mes) {
        return Solver< Kernel<Scalar, Solver> >().solve(mes);
    };

};

}

#endif //DCM_KERNEL_H

