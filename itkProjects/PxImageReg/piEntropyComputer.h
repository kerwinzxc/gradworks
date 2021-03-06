//
//  piEntropyComputer.h
//  ParticleGuidedRegistration
//
//  Created by Joohwi Lee on 2/21/13.
//
//

#ifndef __ParticleGuidedRegistration__piEntropyComputer__
#define __ParticleGuidedRegistration__piEntropyComputer__

#include <iostream>
#include "piImageDef.h"
#include <vnl/algo/vnl_symmetric_eigensystem.h>


namespace pi {

    template <class T>
    class DataIterator3 {
    public:
        T* const databegin;
        T* data;
        T* sample;
        int samplestride;
        int elemstride;

        DataIterator3(): databegin(NULL) {
            data = sample = NULL;
            samplestride = 0;
            elemstride = 0;
        }
        DataIterator3(T* begin, int ns, int ne)
        : databegin(begin), data(begin), sample(begin), samplestride(ns*ne), elemstride(ne) {}

        T& At(int i, int j, int k) {
            data = &databegin[i*samplestride];
            sample = &data[j*elemstride];
            return sample[k];
        }
        T& At(int i, int j) {
            data = &databegin[i*samplestride];
            sample = &data[j*elemstride];
            return *sample;
        }
        T* Data() {
            return data;
        }
        T* Sample() {
            return sample;
        }
        T* NextSample() {
            sample += elemstride;
            return sample;
        }
        T* PrevSample() {
            sample -= elemstride;
            return sample;
        }
        T* FirstSample() {
            sample = data;
            return sample;
        }
        T* PrevData() {
            data -= samplestride;
            sample = data;
            return data;
        }
        T* NextData() {
            data += samplestride;
            sample = data;
            return data;
        }
        T* FirstData() {
            data = databegin;
            sample = data;
            return data;
        }
        T* NextDataSample() {
            sample += samplestride;
            data += samplestride;
            return sample;
        }
        T* PrevDataSample() {
            sample -= samplestride;
            data -= samplestride;
            return sample;
        }
    };

    /**
     * Compute an entropy of multi-dimensional data (nelems * nsamples * ndata)
     * Each sample consists of nelems numuber of elements (# of patchelems)
     * Each data consists of nsample number of samples (# of voxels)
     * Each case consists of ndata number of data (# of images)
     */
    template <class T = double>
    class EntropyComputer {
    public:
        typedef DataIterator3<T> Iterator;
        typedef vnl_matrix<T> Matrix;
        typedef vnl_vector<T> Vector;

        Matrix dataMatrix;
        Iterator dataIter;

        int ndata;
        int nsamples;
        int nelems;
        int ndatasize;

        Vector mean;
        Matrix covariance;
        Matrix inverseCovariance;
        Matrix gradient;

        EntropyComputer(int data, int samples, int elems)
            : dataMatrix(data,samples*elems), dataIter(dataMatrix.data_block(),samples, elems),
                ndata(data), nsamples(samples), nelems(elems), ndatasize(nsamples*nelems) {}
        ~EntropyComputer() {}

        T* GetPointer(int idata, int jsample);
        void MoveToCenter();
        // in dual space
        bool ComputeCovariance(T alpha = 1);
        bool ComputeCovariance(std::vector<bool>& validSamples, T alpha = 1);

        bool ComputeGradient();

        /// @brief Compute the entropy; the determinant of covariance
        double ComputeEntropy();

    private:
        EntropyComputer() :dataIter(NULL,0,0){};
        void operator=(const EntropyComputer& ec) {};
    };


    template <class T>
    T* EntropyComputer<T>::GetPointer(int idata, int jsample) {
        return &dataMatrix[idata][jsample * nelems];
    }

    template <class T>
    void EntropyComputer<T>::MoveToCenter() {
        int ndatasize = nsamples * nelems;
        mean.set_size(ndatasize);
        mean.fill(0);

        Iterator meanIter(mean.data_block(), nsamples, nelems);
        dataIter.FirstData();
        for (int i = 0; i < ndata; i++) {
            for (int j = 0; j < nsamples; j++) {
                for (int k = 0; k < nelems; k++) {
                    meanIter.sample[k] += (dataIter.sample[k]);
                }
                meanIter.NextSample();
                dataIter.NextSample();
            }
            dataIter.NextData();
            meanIter.FirstData();
        }
        mean /= ndata;
        dataIter.FirstData();
        for (int i = 0; i < ndata; i++) {
            for (int j = 0; j < nsamples; j++) {
                for (int k = 0; k < nelems; k++) {
                    dataIter.sample[k] -= meanIter.sample[k];
                }
                meanIter.NextSample();
                dataIter.NextSample();
            }
            dataIter.NextData();
            meanIter.FirstData();
        }
    }


    // in dual space
    template <class T>
    bool EntropyComputer<T>::ComputeCovariance(std::vector<bool>& validSamples, T alpha) {
        if (ndata > nsamples * nelems) {
            std::cout << "not yet implemented" << std::endl;
            return false;
        }

        if (validSamples.size() != nsamples) {
            std::cout << "validSamples.size() != nsamples" << std::endl;
            return false;
        }

        // compute DD'
        int datasize = nsamples * nelems;
        covariance.set_size(ndata, ndata);
        covariance.fill(0);

        Iterator iter1(dataMatrix.data_block(), nsamples, nelems);
        Iterator iter2(dataMatrix.data_block(), nsamples, nelems);

        for (int i = 0; i < ndata; i++) {
            iter2.FirstData();
            for (int j = 0; j < ndata; j++) {
                for (int k = 0; k < datasize; k++) {
                    if (validSamples[k]) {
                        covariance[i][j] += iter1.data[k] * iter2.data[k];
                    }
                }
                iter2.NextData();
            }
            iter1.NextData();
        }

        // this is not working
        // the covariance of two images will be the same
        // ==============================================
        const bool divideByMaxElement = false;
        if (divideByMaxElement) {
            double maxElem = 0;
            for (int i = 0; i < ndata; i++) {
                for (int j = 0; j < ndata; j++) {
                    covariance[i][j] /= (datasize-1);
                    if (covariance[i][j] > maxElem) {
                        maxElem = covariance[i][j];
                    }
                }
            }
            for (int i = 0; i < ndata; i++) {
                for (int j = 0; j < ndata; j++) {
                    covariance[i][j] /= maxElem;
                }
            }
        }
        // ==============================================

        // regularization term
        for (int i = 0; i < ndata; i++) {
            covariance[i][i] += alpha;
        }

        inverseCovariance = vnl_matrix_inverse<T>(covariance);
        if (std::isnan(inverseCovariance[0][0])) {
            std::cout << "Covariance is NaN" << std::endl;
            std::cout << covariance << std::endl;
            inverseCovariance.set_identity();
            return false;
        }
        return true;
    }

    // in dual space
    template <class T>
    bool EntropyComputer<T>::ComputeCovariance(T alpha) {
        if (ndata > nsamples * nelems) {
            std::cout << "not yet implemented" << std::endl;
            return false;
        }

        // compute DD'
        int datasize = nsamples * nelems;
        covariance.set_size(ndata, ndata);
        covariance.fill(0);

        Iterator iter1(dataMatrix.data_block(), nsamples, nelems);
        Iterator iter2(dataMatrix.data_block(), nsamples, nelems);

        for (int i = 0; i < ndata; i++) {
            iter2.FirstData();
            for (int j = 0; j < ndata; j++) {
                for (int k = 0; k < datasize; k++) {
                    covariance[i][j] += iter1.data[k] * iter2.data[k];
                }
                iter2.NextData();
            }
            iter1.NextData();
        }

        // this is not working
        // the covariance of two images will be the same
        const bool divideByMaxElement = false;
        if (divideByMaxElement) {
            double maxElem = 0;
            for (int i = 0; i < ndata; i++) {
                for (int j = 0; j < ndata; j++) {
                    covariance[i][j] /= (datasize-1);
                    if (covariance[i][j] > maxElem) {
                        maxElem = covariance[i][j];
                    }
                }
            }
            for (int i = 0; i < ndata; i++) {
                for (int j = 0; j < ndata; j++) {
                    covariance[i][j] /= maxElem;
                }
            }
        }

        // regularization term
        for (int i = 0; i < ndata; i++) {
            covariance[i][i] += alpha;
        }

        inverseCovariance = vnl_matrix_inverse<T>(covariance);
        if (std::isnan(inverseCovariance[0][0])) {
            std::cout << dataMatrix << std::endl;
            std::cout << "Covariance is NaN" << std::endl;
            std::cout << covariance << std::endl;
            inverseCovariance.set_identity();


            return false;
        }
        return true;
    }




    template <class T>
    bool EntropyComputer<T>::ComputeGradient() {
        // Y*(K+I)^-1
        dataIter.FirstData();
        gradient.set_size(ndata, ndatasize);
        gradient.fill(0);
        Iterator out(gradient.data_block(), ndatasize, 1);
        Iterator in(dataMatrix.data_block(), ndatasize, 1);
        // start from out column

        out.FirstData();
        for (int j = 0; j < ndata; j++) {
            for (int i = 0; i < ndatasize; i++) {
                in.FirstData();
                for (int k = 0; k < ndata; k++) {
                    out.data[i] += in.data[i] * inverseCovariance[k][j];
                    in.NextData();
                }
            }
            out.NextData();
        }
        return true;
    }



    template <class T>
    double EntropyComputer<T>::ComputeEntropy() {
        /// The determinant of the covariance is computed by *vnl_symmetric_eigensystem<>* class
        vnl_symmetric_eigensystem<double> eig(covariance);
        return eig.determinant();
    }
}
#endif /* defined(__ParticleGuidedRegistration__piEntropyComputer__) */
