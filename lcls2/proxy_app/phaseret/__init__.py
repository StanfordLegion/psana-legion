import numpy as np
from numpy import fft
from scipy.ndimage import gaussian_filter

# Convention:
#   a trailing underscore in a numpy array means it is
#   assumed to be shifted for the FFT.


class InitialState:
    """Initial State for the Phaser.

    Allow to set up the initial state of the Phaser.
    In particular, it can be convenient to generate the initial
    support and rho (density estimate).
    """

    def __init__(self, amplitudes, support=None, rho=None,
                 is_ifftshifted=False):
        """Creates the InitialState object.

        :param amplitudes:
            Numpy array with amplitude data
        :param support:
            Numpy array with initial support.
            If None, can be generated by using the relevant functions.
            If left to None when required, one will be automatically
            generated.
        :param rho:
            Numpy array with initial density estimate.
            If None, can be generated by using the relevant functions.
            If left to None when required, one will be automatically
            generated.
        :param is_ifftshifted:
            If True, assume that the arrays have been shifted for the
            fft.
        :return:
        """
        shift = fft.ifftshift if not is_ifftshifted else lambda x: x

        self._amplitudes_ = shift(amplitudes)
        self._shape = self._amplitudes_.shape

        if support is not None:
            self.check_array(support)
            self._support_ = shift(support)
        else:
            self._support_ = None

        if rho is not None:
            self.check_array(rho)
            self._rho_ = shift(rho)
        else:
            self._rho_ = None

    def check_array(self, array):
        if array.shape != self._shape:
            raise ValueError(
                "All provided arrays need to have the same shape")

    def generate_support_from_autocorrelation(self, rel_threshold=0.1):
        intensities_ = self._amplitudes_ ** 2
        autocorrelation_ = np.absolute(fft.fftn(intensities_))
        support_ = \
            autocorrelation_ > rel_threshold * autocorrelation_.max()
        np.copyto(self._support_, support_)

    def generate_random_rho(self):
        support_ = self.get_support(ifftshifted=True)  # In case it's None
        rho_ = support_ * np.random.rand(*support_.shape)
        np.copyto(self._rho_, rho_)

    def get_amplitudes(self, ifftshifted=False):
        shift = fft.fftshift if not ifftshifted else lambda x: x
        return shift(self._amplitudes_)

    def get_support(self, ifftshifted=False):
        if self._support_ is None:
            self.generate_support_from_autocorrelation()
        shift = fft.fftshift if not ifftshifted else lambda x: x
        return shift(self._support_)

    def get_rho(self, ifftshifted=False):
        if self._rho_ is None:
            self.generate_random_rho()
        shift = fft.fftshift if not ifftshifted else lambda x: x
        return shift(self._rho_)


class Phaser:
    """Phasing engine.

    Convenience wrapper around phasing functions.
    """

    def __init__(self, initial_state):
        """Initializes the Phaser.

        :param initial_state:
            Object of type InitialState.
        :return:
        """
        self._amplitudes_ = initial_state.get_amplitudes(ifftshifted=True)
        self._support_ = initial_state.get_support(ifftshifted=True)
        self._rho_ = initial_state.get_rho(ifftshifted=True)

        # Since we cannot predict the number of iterations, we cannot
        # allocate the arrays to store the error measurements.
        # So, we store these measurements in a list of arrays.
        # The arrays can be merged (concatenated) once in a while
        # (e.g. when they are requested by the user).
        self._distF_ls = []
        self._distR_ls = []

    def ER_loop(self, n_loops):
        self._lengthen_arrays(n_loops)
        rho_, _ = ER_loop(
            n_loops, self._rho_, self._amplitudes_, self._support_,
            self._distF_ls[-1], self._distR_ls[-1], 0)
        np.copyto(self._rho_, rho_)

    def HIO_loop(self, n_loops, beta):
        self._lengthen_arrays(n_loops)
        rho_, _ = HIO_loop(
            n_loops, self._rho_, self._amplitudes_, self._support_,
            beta, self._distF_ls[-1], self._distR_ls[-1], 0)
        np.copyto(self._rho_, rho_)

    def _lengthen_arrays(self, n_loops):
        self._distF_ls.append(np.zeros(n_loops))
        self._distR_ls.append(np.zeros(n_loops))

    def shrink_wrap(self, cutoff, sigma=1):
        support_ = shrink_wrap(self._rho_, cutoff, sigma)
        np.copyto(self._support_, support_)

    def get_Fourier_errs(self):
        self._distF_ls = [np.concatenate(self._distF_ls)]
        return self._distF_ls[0]

    def get_real_errs(self):
        self._distR_ls = [np.concatenate(self._distR_ls)]
        return self._distR_ls[0]

    def get_support(self, ifftshifted=False):
        shift = fft.fftshift if not ifftshifted else lambda x: x
        return shift(self._support_)

    def get_rho(self, ifftshifted=False):
        shift = fft.fftshift if not ifftshifted else lambda x: x
        return shift(self._rho_)


def phase(rho_, amplitudes_, support_, distF_l, distR_l, k):
    rho_hat_ = fft.fftn(rho_)
    distF_l[k] = np.linalg.norm(np.absolute(rho_hat_) - amplitudes_) / amplitudes_.size
    phases_ = np.angle(rho_hat_)
    rho_hat_mod_ = amplitudes_ * np.exp(1j*phases_)
    rho_mod_ = fft.ifftn(rho_hat_mod_)
    support_star_ = np.logical_and(support_, rho_mod_>0)
    distR_l[k] = np.linalg.norm(rho_mod_[~support_star_])
    return rho_mod_, support_star_


def ER(rho_, amplitudes_, support_, distF_l, distR_l, k):
    rho_mod_, support_star_ = phase(
        rho_, amplitudes_, support_, distF_l, distR_l, k)
    rho_ = np.where(support_star_, rho_mod_, 0)
    return rho_


def HIO(rho_, amplitudes_, support_, beta, distF_l, distR_l, k):
    rho_mod_, support_star_ = phase(
        rho_, amplitudes_, support_, distF_l, distR_l, k)
    rho_ = np.where(support_star_, rho_mod_, rho_-beta*rho_mod_)
    return rho_


def ER_loop(n_loops, rho_, amplitudes_, support_, distF_l, distR_l, k):
    for ki in range(k, k+n_loops):
        rho_ = ER(rho_, amplitudes_, support_, distF_l, distR_l, ki)
    return rho_, k+n_loops


def HIO_loop(n_loops, rho_, amplitudes_, support_, beta, distF_l, distR_l, k):
    for ki in range(k, k+n_loops):
        rho_ = HIO(rho_, amplitudes_, support_, beta, distF_l, distR_l, ki)
    return rho_, k+n_loops


def shrink_wrap(rho_, cutoff, sigma):
    rho_abs_ = np.absolute(rho_)
    # By using 'wrap', we don't need to fftshift it back and forth
    rho_gauss_ = gaussian_filter(
        rho_abs_, mode='wrap', sigma=sigma, truncate=2)
    support_new_ = rho_gauss_ > rho_abs_.max() * cutoff
    return support_new_

