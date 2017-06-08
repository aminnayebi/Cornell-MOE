/*!
  \file gpp_covariance.hpp
  \rst
  This file specifies CovarianceInterface, the interface for all covariance functions used by the optimal learning
  code base.  It defines three main covariance functions subclassing this interface, Square Exponential, Matern
  with \nu = 1.5 and Matern with \nu = 2.5.  There is also a special isotropic Square Exponential function (i.e., uses
  the same length scale in all dimensions).  We denote a generic covariance function as: ``k(x,x')``

  Covariance functions have a few fundamental properties (see references at the bottom for full details).  In short,
  they are SPSD (symmetric positive semi-definite): ``k(x,x') = k(x', x)`` for any ``x,x'`` and ``k(x,x) >= 0`` for all ``x``.
  As a consequence, covariance matrices are SPD as long as the input points are all distinct.

  Additionally, the Square Exponential and Matern covariances (as well as other functions) are stationary. In essence,
  this means they can be written as ``k(r) = k(|x - x'|) = k(x, x') = k(x', x)``.  So they operate on distances between
  points as opposed to the points themselves.  The name stationary arises because the covariance is the same
  modulo linear shifts: ``k(x+a, x'+a) = k(x, x').``

  Covariance functions are a fundamental component of gaussian processes: as noted in the gpp_math.hpp header comments,
  gaussian processes are defined by a mean function and a covariance function.  Covariance functions describe how
  two random variables change in relation to each other--more explicitly, in a GP they specify how similar two points are.
  The choice of covariance function is important because it encodes our assumptions about how the "world" behaves.

  Currently, all covariance functions in this file require ``dim+1`` hyperparameters: ``\alpha, L_1, ... L_d``. ``\alpha``
  is ``\sigma_f^2``, the signal variance. ``L_1, ... , L_d`` are the length scales, one per spatial dimension.  We do not
  currently support non-axis-aligned anisotropy.

  Specifying hyperparameters is tricky because changing them fundamentally changes the behavior of the GP.
  gpp_model_selection.hpp provides some functions for optimizing
  hyperparameters based on the current training data.

  For more details, see:
  http://en.wikipedia.org/wiki/Covariance_function
  Rasmussen & Williams Chapter 4
\endrst*/

#ifndef MOE_OPTIMAL_LEARNING_CPP_GPP_COVARIANCE_HPP_
#define MOE_OPTIMAL_LEARNING_CPP_GPP_COVARIANCE_HPP_

#include <vector>

#include "gpp_common.hpp"
#include "gpp_exception.hpp"

namespace optimal_learning {

/*!\rst
  Abstract class to enable evaluation of covariance functions--supports the evaluation of the covariance between two
  points, as well as the gradient with respect to those coordinates and gradient/hessian with respect to the
  hyperparameters of the covariance function.

  Covariance operaters, ``cov(x_1, x_2)`` are SPD.  Due to the symmetry, there is no need to differentiate wrt x_1 and x_2; hence
  the gradient operation should only take gradients wrt dim variables, where ``dim = |x_1|``

  Hyperparameters (denoted ``\theta_j``) are stored as class member data by subclasses.

  This class has *only* pure virtual functions, making it abstract. Users cannot instantiate this class directly.
\endrst*/
class CovarianceInterface {
 public:
  virtual ~CovarianceInterface() = default;

  /*!\rst
    Computes the covariance function of the function values and their gradients of two points, cov(``point_one``, ``point_two``).
    Points must be arrays with length dim.

    The covariance function is guaranteed to be symmetric by definition: ``Covariance(x, y) = Covariance(y, x)``.
    This function is also positive definite by definition.

    \param
      :point_one[dim]: first spatial coordinate
      :derivatives_one[dim]: which derivatives of point_one are available
      :point_two[dim]: second spatial coordinate
      :derivatives_two[dim]: which derivatives of point_two are available
    \return
      cov[1+sum(derivatives_one)][1+sum(derivatives_two)]:
      value of covariance between the function values and their gradients of the input points
  \endrst*/
  virtual void Covariance(double const * restrict point_one, int const * restrict derivatives_one, int num_derivatives_one,
                          double const * restrict point_two, int const * restrict derivatives_two, int num_derivatives_two,
                          double * restrict cov) const noexcept OL_WARN_UNUSED_RESULT = 0;

  /*!\rst
    Computes the gradient of this.Covariance(point_one, point_two) with respect to the FIRST argument, point_one.

    This distinction is important for maintaining the desired symmetry.  ``Cov(x, y) = Cov(y, x)``.
    Additionally, ``\pderiv{Cov(x, y)}{x} = \pderiv{Cov(y, x)}{x}``.
    However, in general, ``\pderiv{Cov(x, y)}{x} != \pderiv{Cov(y, x)}{y}`` (NOT equal!  These may differ by a negative sign)

    Hence to avoid separate implementations for differentiating against first vs second argument, this function only handles
    differentiation against the first argument.  If you need ``\pderiv{Cov(y, x)}{x}``, just swap points x and y.

    \param
      :point_one[dim]: first spatial coordinate
      :derivatives_one[dim]: which derivatives of point_one are available
      :point_two[dim]: second spatial coordinate
      :derivatives_two[dim]: which derivatives of point_two are available
    \output
      grad_cov[dim][1+sum(derivatives_one)][1+sum(derivatives_two)]: i-th entry is ``\pderiv{cov(x_1, x_2)}{x1_i}``
  \endrst*/
  virtual void GradCovariance(double const * restrict point_one, int const * restrict derivatives_one, int num_derivatives_one,
                              double const * restrict point_two, int const * restrict derivatives_two, int num_derivatives_two,
                              double * restrict grad_cov) const noexcept OL_NONNULL_POINTERS = 0;



  /*!\rst
    Returns the number of hyperparameters.  This base class only allows for a maximum of dim + 1 hyperparameters but
    subclasses may implement additional ones.

    \return
      The number of hyperparameters.  Return 0 to disable hyperparameter-related gradients, optimizations.
  \endrst*/
  virtual int GetNumberOfHyperparameters() const noexcept OL_PURE_FUNCTION OL_WARN_UNUSED_RESULT = 0;

  /*!\rst
    Similar to GradCovariance(), except gradients are computed wrt the hyperparameters.

    Unlike GradCovariance(), the order of point_one and point_two is irrelevant here (since we are not differentiating against
    either of them).  Thus the matrix of grad covariances (wrt hyperparameters) is symmetric.

    \param
      :point_one[dim]: first spatial coordinate
      :derivatives_one[dim]: which derivatives of point_one are available
      :point_two[dim]: second spatial coordinate
      :derivatives_two[dim]: which derivatives of point_two are available
    \output
      :grad_hyperparameter_cov[this.GetNumberOfHyperparameters()][1+sum(derivatives_one)][1+sum(derivatives_two)]:
      i-th entry is ``\pderiv{cov(x_1, x_2)}{\theta_i}``
  \endrst*/
  virtual void HyperparameterGradCovariance(double const * restrict point_one, int const * restrict derivatives_one, int num_derivatives_one,
                                            double const * restrict point_two, int const * restrict derivatives_two, int num_derivatives_two,
                                            double * restrict grad_hyperparameter_cov) const noexcept OL_NONNULL_POINTERS = 0;

  /*!\rst
    The Hessian matrix of the covariance evaluated at x_1, x_2 with respect to the hyperparameters.  The Hessian is defined as::

      [ \ppderiv{cov}{\theta_0^2}              \mixpderiv{cov}{\theta_0}{\theta_1}    ... \mixpderiv{cov}{\theta_0}{\theta_{n-1}} ]
      [ \mixpderiv{cov}{\theta_1}{\theta_0}    \ppderiv{cov}{\theta_1^2 }             ... \mixpderiv{cov}{\theta_1}{\theta_{n-1}} ]
      [      ...                                                                                     ...                          ]
      [ \mixpderiv{cov}{\theta_{n-1}{\theta_0} \mixpderiv{cov}{\theta_{n-1}{\theta_1} ... \ppderiv{cov}{\theta_{n-1}^2}           ]

    where "cov" abbreviates covariance(x_1, x_2) and "n" refers to the number of hyperparameters.

    Unless noted otherwise in subclasses, the Hessian is symmetric (due to the equality of mixed derivatives when a function
    f is twice continuously differentiable).

    Similarly to the gradients, the Hessian is independent of the order of x_1, x_2: H_{cov}(x_1, x_2) = H_{cov}(x_2, x_1)

    For further details: http://en.wikipedia.org/wiki/Hessian_matrix

    Let n_hyper = this.GetNumberOfHyperparameters()

    \param
      :point_one[dim]: first spatial coordinate
      :derivatives_one[dim]: which derivatives of point_one are available
      :point_two[dim]: second spatial coordinate
      :derivatives_two[dim]: which derivatives of point_two are available
    \output
      :hessian_hyperparameter_cov[n_hyper][n_hyper][1+sum(derivatives_one)][1+sum(derivatives_two)]:
      ``(i,j)``-th entry is ``\mixpderiv{cov(x_1, x_2)}{\theta_i}{\theta_j}``
  \endrst*/
/*  virtual void HyperparameterHessianCovariance(double const * restrict point_one, int const * restrict derivatives_one, int length_one,
                                               double const * restrict point_two, int const * restrict derivatives_two, int length_two,
                                               double * restrict hessian_hyperparameter_cov) const noexcept OL_NONNULL_POINTERS = 0;*/

  /*!\rst
    Sets the hyperparameters.  Hyperparameter ordering is defined implicitly by GetHyperparameters: ``[alpha=\sigma_f^2, length_0, ..., length_{n-1}]``

    \param
      :hyperparameters[this.GetNumberOfHyperparameters()]: hyperparameters to set
  \endrst*/
  virtual void SetHyperparameters(double const * restrict hyperparameters) noexcept OL_NONNULL_POINTERS = 0;

  /*!\rst
    Gets the hyperparameters.  Ordering is ``[alpha=\sigma_f^2, length_0, ..., length_{n-1}]``

    \output
      :hyperparameters[this.GetNumberOfHyperparameters()]: values of current hyperparameters
  \endrst*/
  virtual void GetHyperparameters(double * restrict hyperparameters) const noexcept OL_NONNULL_POINTERS = 0;

  /*!\rst
    For implementing the virtual (copy) constructor idiom.

    \return
      :Pointer to a constructed object that is a subclass of CovarianceInterface
  \endrst*/
  virtual CovarianceInterface * Clone() const OL_WARN_UNUSED_RESULT = 0;
};

/*!\rst
  Implements the square exponential covariance function:
  ``cov(x_1, x_2) = \alpha * \exp(-1/2 * ((x_1 - x_2)^T * L * (x_1 - x_2)) )``
  where L is the diagonal matrix with i-th diagonal entry ``1/lengths[i]/lengths[i]``

  This covariance object has ``dim+1`` hyperparameters: ``\alpha, lengths_i``

  See CovarianceInterface for descriptions of the virtual functions.
\endrst*/
class SquareExponential final : public CovarianceInterface {
 public:
  /*!\rst
    Constructs a SquareExponential object with constant length-scale across all dimensions.

    \param
      :dim: the number of spatial dimensions
      :alpha: the hyperparameter ``\alpha`` (e.g., signal variance, ``\sigma_f^2``)
      :length: the constant length scale to use for all hyperparameter length scales
  \endrst*/
  SquareExponential(int dim, double alpha, double length);

  /*!\rst
    Constructs a SquareExponential object with the specified hyperparameters.

    \param
      :dim: the number of spatial dimensions
      :alpha: the hyperparameter ``\alpha``, (e.g., signal variance, ``\sigma_f^2``)
      :lengths[dim]: the hyperparameter length scales, one per spatial dimension
  \endrst*/
  SquareExponential(int dim, double alpha, double const * restrict lengths) OL_NONNULL_POINTERS;

  /*!\rst
    Constructs a SquareExponential object with the specified hyperparameters.

    \param
      :dim: the number of spatial dimensions
      :alpha: the hyperparameter ``\alpha``, (e.g., signal variance, ``\sigma_f^2``)
      :lengths: the hyperparameter length scales, one per spatial dimension
  \endrst*/
  SquareExponential(int dim, double alpha, std::vector<double> lengths);


  // covariance of point_one and point_two
  virtual void Covariance(double const * restrict point_one, int const * restrict derivatives_one, int num_derivatives_one,
                          double const * restrict point_two, int const * restrict derivatives_two, int num_derivatives_two,
                          double * restrict cov) const noexcept override OL_NONNULL_POINTERS OL_WARN_UNUSED_RESULT;

  // gradient of the covariance wrt point_one (array)
  virtual void GradCovariance(double const * restrict point_one, int const * restrict derivatives_one, int num_derivatives_one,
                              double const * restrict point_two, int const * restrict derivatives_two, int num_derivatives_two,
                              double * restrict grad_cov) const noexcept override OL_NONNULL_POINTERS;

  virtual int GetNumberOfHyperparameters() const noexcept override OL_PURE_FUNCTION OL_WARN_UNUSED_RESULT {
    return 1 + dim_;
  }

  virtual void HyperparameterGradCovariance(double const * restrict point_one, int const * restrict derivatives_one, int num_derivatives_one,
                                            double const * restrict point_two, int const * restrict derivatives_two, int num_derivatives_two,
                                            double * restrict grad_hyperparameter_cov) const noexcept override OL_NONNULL_POINTERS;
/*
  virtual void HyperparameterHessianCovariance(double const * restrict point_one, int const * restrict derivatives_one, int length_one,
                                               double const * restrict point_two, int const * restrict derivatives_two, int length_two,
                                               double * restrict hessian_hyperparameter_cov) const noexcept override OL_NONNULL_POINTERS;*/

  virtual void SetHyperparameters(double const * restrict hyperparameters) noexcept override OL_NONNULL_POINTERS {
    alpha_ = hyperparameters[0];

    hyperparameters += 1;
    for (int i = 0; i < dim_; ++i) {
      lengths_[i] = hyperparameters[i];
      lengths_sq_[i] = Square(hyperparameters[i]);
    }
  }

  virtual void GetHyperparameters(double * restrict hyperparameters) const noexcept override OL_NONNULL_POINTERS {
    hyperparameters[0] = alpha_;

    hyperparameters += 1;
    for (int i = 0; i < dim_; ++i) {
      hyperparameters[i] = lengths_[i];
    }
  }

  virtual CovarianceInterface * Clone() const override OL_WARN_UNUSED_RESULT;

  OL_DISALLOW_DEFAULT_AND_ASSIGN(SquareExponential);

 private:
  explicit SquareExponential(const SquareExponential& source);

  /*!\rst
    Validate and initialize class data members.
  \endrst*/
  void Initialize();

  //! dimension of the problem
  int dim_;
  //! ``\sigma_f^2``, signal variance
  double alpha_;
  //! length scales, one per dimension
  std::vector<double> lengths_;
  //! square of the length scales, one per dimension
  std::vector<double> lengths_sq_;
};

/*!\rst
  Implements the deep additive covariance functionw with 10 deep features
  See CovarianceInterface for descriptions of the virtual functions.
\endrst*/
class DeepAdditiveKernel final : public CovarianceInterface {
 public:

  /*!\rst
    Constructs a SquareExponential object with the specified hyperparameters.
    \param
      :dim: the number of spatial dimensions
      :alpha: the hyperparameter ``\alpha``, (e.g., signal variance, ``\sigma_f^2``)
      :lengths[dim]: the hyperparameter length scales, one per spatial dimension
  \endrst*/
  DeepAdditiveKernel(int dim, double const * restrict hypers) OL_NONNULL_POINTERS;

  /*!\rst
    Constructs a SquareExponential object with the specified hyperparameters.
    \param
      :dim: the number of spatial dimensions
      :alpha: the hyperparameter ``\alpha``, (e.g., signal variance, ``\sigma_f^2``)
      :lengths: the hyperparameter length scales, one per spatial dimension
  \endrst*/
  DeepAdditiveKernel(int dim, std::vector<double> hypers);

  /*!\rst
    deep neural network projection
  \endrst*/
  void NeuralNetwork(double const * restrict point_one, double * restrict projection) const noexcept OL_NONNULL_POINTERS;

  /*!\rst
    the single covariance matrix in the additive kernel
  \endrst*/
  //double AdditiveComponent(double const point_one, double const point_two, int component) const noexcept OL_NONNULL_POINTERS;

  virtual void Covariance(double const * restrict point_one, int const * restrict derivatives_one, int num_derivatives_one,
                          double const * restrict point_two, int const * restrict derivatives_two, int num_derivatives_two,
                          double * restrict cov) const noexcept override OL_NONNULL_POINTERS OL_WARN_UNUSED_RESULT;

  virtual void GradCovariance(double const * restrict point_one, int const * restrict derivatives_one, int num_derivatives_one,
                              double const * restrict point_two, int const * restrict derivatives_two, int num_derivatives_two,
                              double * restrict grad_cov) const noexcept override OL_NONNULL_POINTERS;

  virtual int GetNumberOfHyperparameters() const noexcept override OL_PURE_FUNCTION OL_WARN_UNUSED_RESULT {
    return dim_ * 10 + 10 +
           10 * 10 + 10 +
           10 * 10 + 10 +
           10 * 2;
  }

  virtual void HyperparameterGradCovariance(double const * restrict point_one, int const * restrict derivatives_one, int num_derivatives_one,
                                            double const * restrict point_two, int const * restrict derivatives_two, int num_derivatives_two,
                                            double * restrict grad_hyperparameter_cov) const noexcept override OL_NONNULL_POINTERS{
    OL_THROW_EXCEPTION(LowerBoundException<double>, "Invalid hyperparameter (alpha).", alpha_[0], std::numeric_limits<double>::min());
  }

  virtual void SetHyperparameters(double const * restrict hyperparameters) noexcept override OL_NONNULL_POINTERS {
      std::copy(hyperparameters, hyperparameters + 10*dim_, w_0_.data());
      std::copy(hyperparameters + 10*dim_, hyperparameters + 10*dim_ + 10, b_0_.data());
      std::copy(hyperparameters + 10*dim_ + 10, hyperparameters + 10*dim_ + 10 + 10*10, w_1_.data());
      std::copy(hyperparameters + 10*dim_ + 10 + 10*10, hyperparameters + 10*dim_ + 10 + 10*10 + 10, b_1_.data());
      std::copy(hyperparameters + 10*dim_ + 10 + 10*10 + 10,
                hyperparameters + 10*dim_ + 10 + 10*10 + 10 + 10*10, w_2_.data());
      std::copy(hyperparameters + 10*dim_ + 10 + 10*10 + 10 + 10*10,
                hyperparameters + 10*dim_ + 10 + 10*10 + 10 + 10*10 + 10, b_2_.data());
      std::copy(hyperparameters + 10*dim_ + 10 + 10*10 + 10 + 10*10 + 10,
                hyperparameters + 10*dim_ + 10 + 10*10 + 10 + 10*10 + 10 + 10, alpha_.data());
      std::copy(hyperparameters + 10*dim_ + 10 + 10*10 + 10 + 10*10 + 10 + 10,
                hyperparameters + 10*dim_ + 10 + 10*10 + 10 + 10*10 + 10 + 10 + 10, lengths_.data());
      for (int i = 0; i < dim_; ++i) {
        lengths_sq_[i] = Square(lengths_[i]);
      }
  }

  virtual void GetHyperparameters(double * restrict hyperparameters) const noexcept override OL_NONNULL_POINTERS {
      std::copy(w_0_.data(), w_0_.data() + 10*dim_, hyperparameters);
      std::copy(b_0_.data(), b_0_.data() + 10, hyperparameters + 10*dim_);
      std::copy(w_1_.data(), w_1_.data() + 10*10, hyperparameters + 10*dim_ + 10);
      std::copy(b_1_.data(), b_1_.data() + 10, hyperparameters + 10*dim_ + 10 + 10*10);
      std::copy(w_2_.data(), w_2_.data() + 10*10, hyperparameters + 10*dim_ + 10 + 10*10 + 10);
      std::copy(b_2_.data(), b_2_.data() + 10, hyperparameters + 10*dim_ + 10 + 10*10 + 10 + 10*10);
      std::copy(alpha_.data(), alpha_.data() + 10, hyperparameters + 10*dim_ + 10 + 10*10 + 10 + 10*10 + 10);
      std::copy(lengths_.data(), lengths_.data() + 10, hyperparameters + 10*dim_ + 10 + 10*10 + 10 + 10*10 + 10 + 10);
  }

  virtual DeepAdditiveKernel * Clone() const override OL_WARN_UNUSED_RESULT;

  OL_DISALLOW_DEFAULT_AND_ASSIGN(DeepAdditiveKernel);

 private:
  explicit DeepAdditiveKernel(const DeepAdditiveKernel& source);

  /*!\rst
    Validate and initialize class data members.
  \endrst*/
  void Initialize(std::vector<double> hypers);

  //! dimension of the problem
  int dim_;
  //! hyperparameters for neural network
  std::vector<double> w_0_;
  std::vector<double> b_0_;
  std::vector<double> w_1_;
  std::vector<double> b_1_;
  std::vector<double> w_2_;
  std::vector<double> b_2_;
  //! ``\sigma_f^2``, signal variance
  std::vector<double> alpha_;
  //! length scales, one per dimension
  std::vector<double> lengths_;
  //! square of the length scales, one per dimension
  std::vector<double> lengths_sq_;
};

///*!\rst
//  Special case of the square exponential covariance function where all entries of L must be the same; i.e., all
//  length scales are equal.
//
//  This exists only for testing hyperparameter optimization (since two is an easy number of parameters to work with); in general
//  this class should not be used.
//
//  This covariance object has 2 hyperparameters: ``\alpha, length``
//
//  See CovarianceInterface for descriptions of the virtual functions.
//\endrst*/
//class SquareExponentialSingleLength final : public CovarianceInterface {
// public:
//  /*!\rst
//    Constructs a SquareExponentialSingleLength object. We provide three constructors with signatures matching other
//    covariance classes for convenience.
//
//    \param
//      :dim: the number of spatial dimensions
//      :alpha: the hyperparameter ``\alpha`` (e.g., signal variance, ``\sigma_f^2``)
//      :length: the constant length scale to use for all hyperparameter length scales
//
//    Note: for pointer or vector length, length[0] must be a valid expression.
//  \endrst*/
//  SquareExponentialSingleLength(int dim, double alpha, double length);
//
//  SquareExponentialSingleLength(int dim, double alpha, double const * restrict length) OL_NONNULL_POINTERS;
//
//  SquareExponentialSingleLength(int dim, double alpha, std::vector<double> length);
//
//  // covariance of point_one and point_two
//  virtual void Covariance(double const * restrict point_one, int const * restrict derivatives_one, int length_one,
//                          double const * restrict point_two, int const * restrict derivatives_two, int length_two,
//                          double * restrict cov) const noexcept override OL_NONNULL_POINTERS OL_WARN_UNUSED_RESULT;
//
//  // gradient of the covariance wrt point_one (array)
//  virtual void GradCovariance(double const * restrict point_one, int const * restrict derivatives_one, int length_one,
//                              double const * restrict point_two, int const * restrict derivatives_two, int length_two,
//                              double * restrict grad_cov) const noexcept override OL_NONNULL_POINTERS;
//
//  virtual int GetNumberOfHyperparameters() const noexcept override OL_PURE_FUNCTION OL_WARN_UNUSED_RESULT {
//    return 2;
//  }
//
//  virtual void HyperparameterGradCovariance(double const * restrict point_one, int const * restrict derivatives_one, int length_one,
//                                            double const * restrict point_two, int const * restrict derivatives_two, int length_two,
//                                            double * restrict grad_hyperparameter_cov) const noexcept override OL_NONNULL_POINTERS;
//
//  virtual void HyperparameterHessianCovariance(double const * restrict point_one, int const * restrict derivatives_one, int length_one,
//                                               double const * restrict point_two, int const * restrict derivatives_two, int length_two,
//                                               double * restrict hessian_hyperparameter_cov) const noexcept override OL_NONNULL_POINTERS;
//
//  virtual void SetHyperparameters(double const * restrict hyperparameters) noexcept override OL_NONNULL_POINTERS {
//    alpha_ = hyperparameters[0];
//    length_ = hyperparameters[1];
//    length_sq_ = Square(hyperparameters[1]);
//  }
//
//  virtual void GetHyperparameters(double * restrict hyperparameters) const noexcept override OL_NONNULL_POINTERS {
//    hyperparameters[0] = alpha_;
//    hyperparameters[1] = length_;
//  }
//
//  virtual CovarianceInterface * Clone() const override OL_WARN_UNUSED_RESULT;
//
//  OL_DISALLOW_DEFAULT_AND_ASSIGN(SquareExponentialSingleLength);
//
// private:
//  explicit SquareExponentialSingleLength(const SquareExponentialSingleLength& source);
//
//  //! dimension of the problem
//  int dim_;
//  //! ``\sigma_f^2``, signal variance
//  double alpha_;
//  //! length scale, one for all dimensions
//  double length_;
//  //! square of the length scale
//  double length_sq_;
//};

///*!\rst
//  Implements a case of the Matern class of covariance functions:
//  ``cov_{matern}(r) = \alpha [\frac{2^{1-\nu}}{\Gamma(\nu)}\left( \frac{\sqrt{2\nu}r}{l} \right)^{\nu} B_{\nu}\left( \frac{\sqrt{2\nu}r}{l} \right)]``
//  where \nu is the "smoothness parameter", ``l`` is the length-scale, ``r = x_1 - x_2``, and ``B_{\nu}`` is a modified Bessel Function.
//
//  Note that for nonconstant (over dimensions) length scales, ``r_i = (x_1_i - x_2_i)/l_i``.  The quantity ``\frac{r}{l}`` will implicitly
//  represent this component-wise division.
//
//  This class implements \nu = 3/2, which simplifies the previous expression to:
//  ``cov_{\nu=3/2}(r) = (1 + \sqrt{3}\frac{r}[l})\exp(-\sqrt{3}\frac{r}{l})``
//
//  This covariance object has ``dim+1`` hyperparameters: ``\alpha, lengths_i``
//
//  See CovarianceInterface for descriptions of the virtual functions.
//\endrst*/
//class MaternNu1p5 final : public CovarianceInterface {
// public:
//  /*!\rst
//    Constructs a MaternNu1p5 object with constant length-scale across all dimensions.
//
//    \param
//      :dim: the number of spatial dimensions
//      :alpha: the hyperparameter ``\alpha`` (e.g., signal variance, ``\sigma_f^2``)
//      :length: the constant length scale to use for all hyperparameter length scales
//  \endrst*/
//  MaternNu1p5(int dim, double alpha, double length);
//
//  /*!\rst
//    Constructs a MaternNu1p5 object with the specified hyperparameters.
//
//    \param
//      :dim: the number of spatial dimensions
//      :alpha: the hyperparameter ``\alpha``, (e.g., signal variance, ``\sigma_f^2``)
//      :lengths[dim]: the hyperparameter length scales, one per spatial dimension
//  \endrst*/
//  MaternNu1p5(int dim, double alpha, double const * restrict lengths) OL_NONNULL_POINTERS;
//
//  /*!\rst
//    Constructs a MaternNu1p5 object with the specified hyperparameters.
//
//    \param
//      :dim: the number of spatial dimensions
//      :alpha: the hyperparameter ``\alpha``, (e.g., signal variance, ``\sigma_f^2``)
//      :lengths: the hyperparameter length scales, one per spatial dimension
//  \endrst*/
//  MaternNu1p5(int dim, double alpha, std::vector<double> lengths);
//
//  // covariance of point_one and point_two
//  virtual void Covariance(double const * restrict point_one, int const * restrict derivatives_one, int length_one,
//                          double const * restrict point_two, int const * restrict derivatives_two, int length_two,
//                          double * restrict cov) const noexcept override OL_NONNULL_POINTERS OL_WARN_UNUSED_RESULT;
//
//  // gradient of the covariance wrt point_one (array)
//  virtual void GradCovariance(double const * restrict point_one, int const * restrict derivatives_one, int length_one,
//                              double const * restrict point_two, int const * restrict derivatives_two, int length_two,
//                              double * restrict grad_cov) const noexcept override OL_NONNULL_POINTERS;
//
//  virtual int GetNumberOfHyperparameters() const noexcept override OL_PURE_FUNCTION OL_WARN_UNUSED_RESULT {
//    return dim_ + 1;
//  }
//
//  // hyperparameter gradients
//  virtual void HyperparameterGradCovariance(double const * restrict point_one, int const * restrict derivatives_one, int length_one,
//                                            double const * restrict point_two, int const * restrict derivatives_two, int length_two,
//                                            double * restrict grad_hyperparameter_cov) const noexcept override OL_NONNULL_POINTERS;
//
//  virtual void HyperparameterHessianCovariance(double const * restrict point_one, int const * restrict derivatives_one, int length_one,
//                                               double const * restrict point_two, int const * restrict derivatives_two, int length_two,
//                                               double * restrict hessian_hyperparameter_cov) const noexcept override OL_NONNULL_POINTERS;
//
//  virtual void SetHyperparameters(double const * restrict hyperparameters) noexcept override OL_NONNULL_POINTERS {
//    alpha_ = hyperparameters[0];
//
//    hyperparameters += 1;
//    for (int i = 0; i < dim_; ++i) {
//      lengths_[i] = hyperparameters[i];
//      lengths_sq_[i] = Square(hyperparameters[i]);
//    }
//  }
//
//  virtual void GetHyperparameters(double * restrict hyperparameters) const noexcept override OL_NONNULL_POINTERS {
//    hyperparameters[0] = alpha_;
//    hyperparameters += 1;
//    for (int i = 0; i < dim_; ++i) {
//      hyperparameters[i] = lengths_[i];
//    }
//  }
//
//  virtual CovarianceInterface * Clone() const override OL_WARN_UNUSED_RESULT;
//
//  OL_DISALLOW_DEFAULT_AND_ASSIGN(MaternNu1p5);
//
// private:
//  explicit MaternNu1p5(const MaternNu1p5& source);
//
//  /*!\rst
//    Validate and initialize class data members.
//  \endrst*/
//  void Initialize();
//
//  //! dimension of the problem
//  int dim_;
//  //! ``\sigma_f^2``, signal variance
//  double alpha_;
//  //! length scales, one per dimension
//  std::vector<double> lengths_;
//  //! square of the length scales, one per dimension
//  std::vector<double> lengths_sq_;
//};

///*!\rst
//  Implements a case of the Matern class of covariance functions with \nu = 5/2 (smoothness parameter).
//  See docs for ``MaternNu1p5`` for more details on the Matern class of covariance fucntions.
//
//  ``cov_{\nu=5/2}(r) = (1 + \sqrt{5}\frac{r}[l} + \frac{5}{3}\frac{r^2}{l^2})\exp(-\sqrt{5}\frac{r}{l})``
//
//  See CovarianceInterface for descriptions of the virtual functions.
//\endrst*/
//class MaternNu2p5 final : public CovarianceInterface {
// public:
//  /*!\rst
//    Constructs a MaternNu2p5 object with constant length-scale across all dimensions.
//
//    \param
//      :dim: the number of spatial dimensions
//      :alpha: the hyperparameter ``\alpha`` (e.g., signal variance, ``\sigma_f^2``)
//      :length: the constant length scale to use for all hyperparameter length scales
//  \endrst*/
//  MaternNu2p5(int dim, double alpha, double length);
//
//  /*!\rst
//    Constructs a MaternNu2p5 object with the specified hyperparameters.
//
//    \param
//      :dim: the number of spatial dimensions
//      :alpha: the hyperparameter ``\alpha``, (e.g., signal variance, ``\sigma_f^2``)
//      :lengths[dim]: the hyperparameter length scales, one per spatial dimension
//  \endrst*/
//  MaternNu2p5(int dim, double alpha, double const * restrict lengths) OL_NONNULL_POINTERS;
//
//  /*!\rst
//    Constructs a MaternNu2p5 object with the specified hyperparameters.
//
//    \param
//      :dim: the number of spatial dimensions
//      :alpha: the hyperparameter ``\alpha``, (e.g., signal variance, ``\sigma_f^2``)
//      :lengths: the hyperparameter length scales, one per spatial dimension
//  \endrst*/
//  MaternNu2p5(int dim, double alpha, std::vector<double> lengths);
//
//  // covariance of point_one and point_two
//  virtual void Covariance(double const * restrict point_one, int const * restrict derivatives_one, int length_one,
//                          double const * restrict point_two, int const * restrict derivatives_two, int length_two,
//                          double * restrict cov) const noexcept override OL_NONNULL_POINTERS OL_WARN_UNUSED_RESULT;
//
//  // gradient of the covariance wrt point_one (array)
//  virtual void GradCovariance(double const * restrict point_one, int const * restrict derivatives_one, int length_one,
//                              double const * restrict point_two, int const * restrict derivatives_two, int length_two,
//                              double * restrict grad_cov) const noexcept override OL_NONNULL_POINTERS;
//
//  // number of hyperparameters
//  virtual int GetNumberOfHyperparameters() const noexcept override OL_PURE_FUNCTION OL_WARN_UNUSED_RESULT {
//    return dim_ + 1;
//  }
//
//  // hyperparameter gradients
//  virtual void HyperparameterGradCovariance(double const * restrict point_one, int const * restrict derivatives_one, int length_one,
//                                            double const * restrict point_two, int const * restrict derivatives_two, int length_two,
//                                            double * restrict grad_hyperparameter_cov) const noexcept override OL_NONNULL_POINTERS;
//
//  virtual void HyperparameterHessianCovariance(double const * restrict point_one, int const * restrict derivatives_one, int length_one,
//                                               double const * restrict point_two, int const * restrict derivatives_two, int length_two,
//                                               double * restrict hessian_hyperparameter_cov) const noexcept override OL_NONNULL_POINTERS;
//
//  virtual void SetHyperparameters(double const * restrict hyperparameters) noexcept override OL_NONNULL_POINTERS {
//    alpha_ = hyperparameters[0];
//
//    hyperparameters += 1;
//    for (int i = 0; i < dim_; ++i) {
//      lengths_[i] = hyperparameters[i];
//      lengths_sq_[i] = Square(hyperparameters[i]);
//    }
//  }
//
//  virtual void GetHyperparameters(double * restrict hyperparameters) const noexcept override OL_NONNULL_POINTERS {
//    hyperparameters[0] = alpha_;
//    hyperparameters += 1;
//    for (int i = 0; i < dim_; ++i) {
//      hyperparameters[i] = lengths_[i];
//    }
//  }
//
//  virtual CovarianceInterface * Clone() const override;
//
// private:
//  explicit MaternNu2p5(const MaternNu2p5& source);
//
//  /*!\rst
//    Validate and initialize class data members.
//  \endrst*/
//  void Initialize();
//
//  //! dimension of the problem
//  int dim_;
//  //! ``\sigma_f^2``, signal variance
//  double alpha_;
//  //! length scales, one per dimension
//  std::vector<double> lengths_;
//  //! square of the length scales, one per dimension
//  std::vector<double> lengths_sq_;
//
//  OL_DISALLOW_DEFAULT_AND_ASSIGN(MaternNu2p5);
//};

}  // end namespace optimal_learning

#endif  // MOE_OPTIMAL_LEARNING_CPP_GPP_COVARIANCE_HPP_
