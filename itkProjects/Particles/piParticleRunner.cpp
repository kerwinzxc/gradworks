//
//  piParticleRunner.cpp
//  ParticleGuidedRegistration
//
//  Created by Joohwi Lee on 10/25/13.
//
//

#include "piImageDef.h"
#include "piParticleRunner.h"
#include "piPatchCompare.h"
#include "piImageIO.h"
#include "piImageProcessing.h"
#include "piParticleWarp.h"
#include "piEntropyComputer.h"

#include <numeric>
#include <algorithm>
#include <itkResampleImageFilter.h>
#include <itkSmoothingRecursiveGaussianImageFilter.h>
#include <itkVectorResampleImageFilter.h>

using namespace libconfig;
using namespace std;

namespace pi {
    typedef itk::ResampleImageFilter<RealImage, RealImage> ResampleImageFilterType;

    static PxTools sys;
    static ImageIO<RealImage> io;
    static ImageIO<LabelImage> labelIO;

    std::vector<PatchImage::Pointer> __patchImages;

    template <typename T>
    typename T::Pointer applyGaussian(typename T::Pointer input, double sigma) {
        typedef itk::SmoothingRecursiveGaussianImageFilter<T> GaussianFilter;
        typename GaussianFilter::Pointer filter = GaussianFilter::New();
        filter->SetSigma(sigma);
        filter->SetInput(input);
        filter->Update();
        typename T::Pointer output = filter->GetOutput();
        output->DisconnectPipeline();
        return output;
    }

    LabelImage3::Pointer createImage3(LabelImage::Pointer refImage, int m) {
        ImageIO<LabelImage3> io;
        LabelImage::SizeType sz = refImage->GetBufferedRegion().GetSize();
        LabelImage3::Pointer tracker = io.NewImageT(sz[0], sz[1], m);
        LabelImage3::SpacingType spacing;
        LabelImage3::PointType origin;
        LabelImage3::DirectionType direction;
        direction.Fill(0);
        fordim (k) {
            spacing[k] = refImage->GetSpacing()[k];
            origin[k] = refImage->GetOrigin()[k];
            fordim (l) {
                direction[k][l] = refImage->GetDirection()[k][l];
            }
        }
        spacing[2] = spacing[0];
        origin[2] = 0;
        direction[2][2] = 1;

        tracker->FillBuffer(0);
        tracker->SetSpacing(spacing);
        tracker->SetOrigin(origin);
        tracker->SetDirection(direction);

        return tracker;
    }

    void executeParticleRunner(pi::Options &opts, StringVector &args) {
        ParticleRunner runner;
        runner.main(opts, args);
    }


    // utility operator overloading
    ostream& operator<<(ostream& os, const Px& par) {
        fordim(k) { os << par.x[k] << " "; }
        return os;
    }
    ostream& operator<<(ostream& os, const Px::Vector& par) {
        Px::Vector::const_iterator p = par.begin();

        if (p == par.end()) {
            return os;
        }

        os << *p;
        for (p++; p != par.end(); p++) {
            os << endl << *p;
        }

        return os;
    }
    ostream& operator<<(ostream& os, const PxSubj& p) {
        for (int i = 0; i < p.size(); i++) {
            cout << p.particles[i] << endl;
        }
        return os;
    }

    // load label map, distance map, gradient map for each region
    bool PxR::load(libconfig::Setting& cache, bool error) {
        string labelfile = cache[0];
        string distfile = cache[1];
        string gradfile = cache[2];


        if (sys.checkFile(labelfile)) {
            labelmap = io.ReadImageS<LabelImage>(labelfile);
        } else {
            if (error) {
                cout << "can't read " << labelfile << endl;
            }
            return false;
        }
        if (sys.checkFile(distfile)) {
            distmap = io.ReadImageS<VectorImage>(distfile);
        }
        if (sys.checkFile(gradfile)) {
            gradmap = io.ReadImageS<GradientImage>(gradfile);
        }

        deriveImages(cache, labelmap, false);
        return true;
    }

    void PxR::deriveImages(libconfig::Setting& cache, LabelImage::Pointer label, bool saveLabel) {
        string labelfile = cache[0];
        string distfile = cache[1];
        string gradfile = cache[2];

        ImageProcessing proc;
        if (saveLabel) {
            labelmap = label;
            io.WriteImageS<LabelImage>(labelfile, label);
        }
        if (distmap.IsNull()) {
            distmap = proc.ComputeDistanceMap(label);
            io.WriteImageS<VectorImage>(distfile, distmap);
        }
        if (gradmap.IsNull()) {
            LabelImage::SpacingType spacing = label->GetSpacing();
            gradmap = proc.ComputeGaussianGradient(label, spacing[0] / 2.0);
            io.WriteImageS<GradientImage>(gradfile, gradmap);
        }

        lablIntp = LabelImageInterpolatorType::New();
        lablIntp->SetInputImage(labelmap);

        distIntp = LinearVectorImageInterpolatorType::New();
        distIntp->SetInputImage(distmap);

        gradIntp = GradientImageInterpolatorType::New();
        gradIntp->SetInputImage(gradmap);

    }

    void PxR::computeNormal(Px& p, Px& nOut) {
        GradientImageInterpolatorType::InputType pi;
        fordim (k) {
            pi[k] = p[k];
        }
        GradientImageInterpolatorType::OutputType nx = gradIntp->Evaluate(pi);
        fordim (k) {
            nOut[k] = nx[k];
        }
    }

    bool PxR::isIn(Px& p) {
        LabelImageInterpolatorType::InputType pi;
        fordim (k) {
            pi[k] = p[k];
        }
        if (lablIntp->IsInsideBuffer(pi) && lablIntp->Evaluate(pi) > 0) {
            return true;
        }
        return false;
    }

    bool PxR::projectParticle(Px& p) {
        if (p.isnan()) {
            return false;
        }
        LabelImage::PointType px;
        fordim (k) {
            px[k] = p[k];
        }
        LabelImage::IndexType ix;
        labelmap->TransformPhysicalPointToIndex(px, ix);
        if (!lablIntp->IsInsideBuffer(ix)) {
            return false;
        }
        if (isIn(p)) {
            return false;
        }
        LinearVectorImageInterpolatorType::InputType pi;
        LinearVectorImageInterpolatorType::OutputType dx = distIntp->EvaluateAtIndex(ix);
        fordim (k) {
            ix[k] += dx[k];
        }
        labelmap->TransformIndexToPhysicalPoint(ix, px);
        fordim (k) {
            p[k] = px[k];
        }
        return true;
    }

    bool PxR::normalForce(Px& p, Px& f, Px& fout) {
        if (isIn(p)) {
            return false;
        }

        Px n = 0;
        computeNormal(p, n);

        fordim (k) {
            fout[k] = n[k] * abs(f[k]);
        }
        return true;
    }


    LabelImage::IndexType PxSubj::getIndex(int i) {
        LabelImage::Pointer regionLabel = regions[attrs[i].label].labelmap;
        LabelImage::PointType px;
        fordim (k) {
            px[k] = particles[i][k];
        }
        LabelImage::IndexType ix;
        regionLabel->TransformPhysicalPointToIndex(px, ix);
        return ix;
    }



    /// inline function
    inline void computeRepulsionWeight(VNLMatrix& weights, int i, int j, Px& pi, Px& pj, double cutoff, double sigma2, double kappa) {
        // compute weight
        double dij = pi.dist(pj);
        if (dij < cutoff) {
            weights[i][j] = exp(-dij*dij*kappa/(sigma2)) / dij;
        } else {
            weights[i][j] = 0;
        }
    }

    // compute repulsion force
    void PxSubj::computeRepulsion(bool ignoreNeighbors) {
        const int npx = size();
        VNLMatrix weights(npx, npx);
        weights.fill(0);

        // loop over all particle pairs
        for (int i = 0; i < npx; i++) {
            Px& pi = particles[i];
            weights[i][i] = 0;
            double kappa = 1;

            const int label = attrs[i].label;
            double sigma = global->sigmaParams[label];
            double cutoff = global->cutoffParams[label];
            const double sigma2 = sigma * sigma;

            // loop over neighbors
            bool useNeighbors = global->neighbors.size() == npx && global->useLocalRepulsion;
            if (!useNeighbors || ignoreNeighbors) {
                // neighbor structure isn't yet built (sampler case)
                for (int jj = i + 1; jj < npx; jj++) {
                    Px& pj = particles[jj];
                    computeRepulsionWeight(weights, i, jj, pi, pj, cutoff, sigma2, kappa);
                    weights[jj][i] = weights[i][jj];
                }
            } else {
                for (int j = 0; j < global->neighbors[i].size(); j++) {
                    int jj = global->neighbors[i][j];
                    Px& pj = particles[jj];
                    computeRepulsionWeight(weights, i, jj, pi, pj, cutoff, sigma2, kappa);
                }
            }
        }

        // compute force
        VNLVector wsum(npx);
        for (int i = 0; i < npx; i++) {
            wsum[i] = 0;
            for (int j = 0; j < npx; j++) {
                wsum[i] += weights[i][j];
            }
        }
        for (int i = 0; i < npx; i++) {
            Px& fi = forces[i];
            Px& pi = particles[i];
            const int label = attrs[i].label;
            double coeff = global->repulsionCoeff[label];
            for (int j = i + 1; j < npx; j++) {
                Px& pj = particles[j];
                Px& fj = forces[j];
                double c = coeff * weights[i][j];
                if (wsum[i] > 1e-5 && wsum[j] > 1e-5) {
                    fordim (k) {
                        double fk = c * (pi.x[k] - pj.x[k]);
                        fi.x[k] += fk / wsum[i];
                        fj.x[k] -= fk / wsum[j];
                    }
                }
            }
        }
    }


    // update the particle dynamic equation
    void PxSubj::updateSystem(double dt) {
        Px::Vector::iterator p = particles.begin();
        Px::Vector::iterator f = forces.begin();
        Px::Vector::iterator e = ensembleForces.begin();

        for (; p != particles.end() && f != forces.end(); p++, f++, e++) {
            fordim (k) { p->x[k] += (dt * (f->x[k] + e->x[k])); }
        }
    }

    // iterate all particles and move inside of the object
    void PxSubj::constrainParticles() {
        Px::Vector::iterator p = particles.begin();
        PxA::Vector::iterator a = attrs.begin();
        for (; p != particles.end(); a++, p++) {
            a->bound = regions[a->label].projectParticle(*p);
        }
    }

    // iterate all particles and remove boundary normal component of forces
    void PxSubj::constrainForces() {
        Px::Vector::iterator p = particles.begin();
        Px::Vector::iterator f = forces.begin();
        PxA::Vector::iterator a = attrs.begin();
        Px fn = 0;
        for (; f != forces.end(); p++, a++, f++) {
            if (a->bound) {
                if (regions[a->label].normalForce(*p, *f, fn)) {
                    *f += fn;
                }
            }
        }
    }

    // random sample particles from each region
    // this function is only for the sampler
    void PxSubj::sampleParticles(std::vector<int>& numParticles) {
        // allocate particles
        int totalParticles = std::accumulate(numParticles.begin(), numParticles.end(), 0);
        resize(totalParticles);

        // assign labels
        int total = 0, label = 0;
        for (int i = 0; i < numParticles.size(); i++) {
            for (int j = 0; j < numParticles[i]; j++) {
                attrs[total].label = label;
                total ++;
            }
            label ++;
        }

        // loop over labels
        total = 0;
        for (unsigned int i = 0; i < regions.size(); i++) {
            // identify avaiable pixels
            std::vector<LabelImage::IndexType> pixelIds;
            LabelImageIteratorType iter(regions[i].labelmap, regions[i].labelmap->GetBufferedRegion());

            for (iter.GoToBegin(); !iter.IsAtEnd(); ++iter) {
                if (iter.Get() > 0) {
                    pixelIds.push_back(iter.GetIndex());
                }
            }

            // shuffle avaiable pixels and choose some
            const int npx = numParticles[i];
            std::random_shuffle(pixelIds.begin(), pixelIds.end());

            for (int j = 0; j < npx; j++) {
                LabelImage::PointType pts;
                regions[i].labelmap->TransformIndexToPhysicalPoint(pixelIds[j], pts);

                attrs[total].label = i;
                particles[total].x[0] = pts[0];
                particles[total].x[1] = pts[1];

                total ++;
            }
        }
    }


    // load particle coordinates and labels from libconfig format
    bool PxSubj::load(string filename) {
        if (!sys.checkFile(filename)) {
            return false;
        }

        libconfig::Config config;
        config.readFile(filename.c_str());

        Setting& labels = config.lookup("labels");
        Setting& data = config.lookup("particles");

        if (global->totalParticles != labels.getLength() || labels.getLength() != data[0].getLength()) {
            return false;
        }

        int npx = labels.getLength();
        resize(npx);
        for (int i = 0; i < npx; i++) {
            attrs[i].label = labels[i];
            fordim (k) {
                particles[i][k] = data[k][i];
            }
        }
        return true;
    }

    // save particle coordinates and labels in libconfig format
    void PxSubj::save(ostream &os) {
        os << "labels = [" << attrs[0].label;
        for (int i = 1; i < attrs.size(); i++) {
            os << "," << attrs[i].label;
        }
        os << "]" << endl;

        os.setf(ios::fixed, ios::floatfield);
        os << "particles = (";

        fordim (k) {
            if (k > 0) {
                os << ",";
            }
            os << "[" << particles[0][k];
            for (int i = 1; i < attrs.size(); i++) {
                os << "," << particles[i][k];
            }
            os << "]" << endl;
        }
        os << ")" << endl;
    }

#pragma mark PxEnsemble Implementations
    void PxAffine::estimateAffineTransform(pi::PxSubj *b) {
        VNLDoubleMatrix mat(_p->size(), __Dim);
        VNLDoubleMatrix mbt(_p->size(), __Dim);
        for (int i = 0; i < mat.rows(); i++) {
            fordim (k) {
                mbt[i][k] = (*_p)[i][k] - 51;
                mat[i][k] = b->particles[i][k] - 51;
            }
        }
        double ma = 0, mb = 0;
        for (int i = 0; i < mat.rows(); i++) {
            ma += mat[i][0]*mat[i][0] + mat[i][1]*mat[i][1];
            mb += mbt[i][0]*mbt[i][0] + mbt[i][1]*mbt[i][1];
        }
        ma /= mat.rows();
        mb /= mbt.rows();
        for (int i = 0; i < mat.rows(); i++) {
            fordim (k) {
                mat[i][k] /= ma;
                mbt[i][k] /= mb;
            }
        }
        VNLDoubleMatrix m = mbt.transpose() * mat;

        vnl_svd<double> svd(m);
        VNLDoubleMatrix v = svd.V();
        VNLDoubleMatrix u = svd.U();
        this->r = v.transpose() * u;

        double rm = sqrt(r[0][0]*r[0][0] + r[0][1]*r[0][1]);
        r /= rm;

        for (int i = 0; i < mat.rows(); i++) {
            (*_q)[i][0] = ma * (r[0][0] * mat[i][0] + r[0][1] * mat[i][1]) + 51;
            (*_q)[i][1] = ma * (r[1][0] * mat[i][0] + r[1][1] * mat[i][1]) + 51;
        }
    }

    void PxAffine::transformVector(Px::Elem *f, Px::Elem *fOut) {
        fordim (i) {
            fOut[i] = 0;
            fordim (j) {
                fOut[i] += r[i][j] * f[j];
            }
        }
    }

    void PxEnsemble::computeAttraction(PxSubj::Vector &subjs) {
        // compute entropy for each particle
        EntropyComputer<double> comp(global->nsubjs, global->totalParticles, __Dim);
        // copy data into the entropy computer
        comp.dataIter.FirstData();
        for (int i = 0; i < global->nsubjs; i++) {
            for (int j = 0; j < global->totalParticles; j++) {
                fordim (k) {
                    comp.dataIter.sample[k] = subjs[i].affineAligned[j][k];
                }
                comp.dataIter.NextSample();
            }
            comp.dataIter.NextData();
        }
        // move to center (subtract mean from each sample)
        comp.MoveToCenter();
        // compute covariance matrix
        comp.ComputeCovariance();
        // compute inverse covariance
        comp.ComputeGradient();

        // now compute gradient w.r.t particles
        for (int i = 0; i < global->nsubjs; i++) {
            subjs[0].clearVector(subjs[i].ensembleForces);
        }


        EntropyComputer<double>::Iterator gradIter(comp.gradient.data_block(), global->totalParticles, __Dim);
        gradIter.FirstData();
        for (int i = 1; i < global->nsubjs; i++) {
            for (int j = 0; j < global->totalParticles; j++) {
                double coeff = global->ensembleCoeff[subjs[i].attrs[j].label];
                Px ensembleForce;
                subjs[i].affineTxf.transformVector(gradIter.sample, ensembleForce.x);
                fordim (k) {
                    subjs[i].ensembleForces[j][k] += coeff*ensembleForce[k];
                }
                gradIter.NextSample();
            }
            gradIter.NextData();
        }
    }


    void PxImageTerm::load(libconfig::Setting& subjects) {
        // # of subjects must be same
        assert(global->nsubjs == subjects.getLength());
        images.clear();

        // iterate over # of subjects
        for (int i = 0; i < global->nsubjs; i++) {
            string imageFile;
            assert(subjects[i].lookupValue("images.[0]", imageFile));
            RealImage::Pointer image = io.ReadCastedImage(imageFile);
            images.push_back(image);
        }
    }

    class PxPatchSampler {
    public:
        typedef RealImage::PixelType PixelType;
        typedef RealImage::IndexType IndexType;
        typedef RealImage::PointType PointType;
        typedef RealImage::RegionType RegionType;
        typedef RealImage::SizeType SizeType;

        PxPatchSampler(RegionType region, RealImage::Pointer image);
        ~PxPatchSampler();

        void setSampleRegion(RegionType& region, RealImage* reference);

        void sampleValues(LinearImageInterpolatorType* interp, Px& px, PxImageTerm::PixelVector& pxValue);
        void sampleGradients(GradientInterpolatorType* interp, Px& px, Px::Vector& pxGrad);

        void createSampleIndexes2(IndexType& startIdx);
        void createSampleIndexes3(IndexType& startIdx);

    private:
        RealImage::SizeType _regionSize;

        int _numberOfSamples;
        std::vector<IndexType> _indexes;
        std::vector<PointType> _points;
    };

    void PxImageTerm::computeImageTerm(PxSubj::Vector& subjs, int w) {
        const int npx = global->totalParticles;
        const int nsx = global->nsubjs;
        const int nex = (__Dim == 2) ? w * w : w * w * w;

        // compute individual entropy
        EntropyComputer<double> comp(nsx, npx, nex);

        // rewind data
        comp.dataIter.FirstData();
        for (int i = 0; i < nsx; i++) {
            for (int j = 0; j < npx; j++) {
                comp.dataIter.NextSample();
            }
            comp.dataIter.NextData();
        }

        // move to center (subtract mean from each sample)
        comp.MoveToCenter();
        // compute covariance matrix
        comp.ComputeCovariance();
        // compute inverse covariance
        comp.ComputeGradient();

        // now compute gradient w.r.t particles
        for (int i = 0; i < global->nsubjs; i++) {
            subjs[0].clearVector(subjs[i].ensembleForces);
        }
        /*
        EntropyComputer<double>::Iterator gradIter(comp.gradient.data_block(), global->totalParticles, __Dim);
        gradIter.FirstData();
        for (int i = 1; i < global->nsubjs; i++) {
            for (int j = 0; j < global->totalParticles; j++) {


                double coeff = global->ensembleCoeff[subjs[i].attrs[j].label];
                fordim (d) {
                    subjs[i].imageForces[j][d] = 0;
                    for (int k = 0; k < nex; k++) {
//                        subjs[i].imageForces[j][d] += gradIter.sample[k] * grad[j][k];
                    }
                }
                gradIter.NextSample();
            }
            gradIter.NextData();
        }
         */
    }

#pragma mark PxSystem Implementations

    void PxGlobal::load(ConfigFile& config) {
        nsubjs = config["particles.number-of-subjects"];
        nlabels = config["particles.number-of-labels"];

        useLocalRepulsion = false;
        config["particles"].lookupValue("use-local-repulsion", useLocalRepulsion);

        Setting& settingNumParticles = config["particles.number-of-particles"];
        for (int i = 0; i < settingNumParticles.getLength(); i++) {
            numParticles.push_back(settingNumParticles[i]);
        }
        totalParticles = std::accumulate(numParticles.begin(), numParticles.end(), 0);

        Setting& params = config["particles.parameters"];
        repulsionCoeff.resize(nlabels);
        ensembleCoeff.resize(nlabels);
        sigmaParams.resize(nlabels);
        cutoffParams.resize(nlabels);
        for (int i = 0; i < params.getLength(); i++) {
            repulsionCoeff[i] = params[i][0];
            ensembleCoeff[i] = params[i][1];
            sigmaParams[i] = params[i][2];
            cutoffParams[i] = params[i][3];
        }
    }

    void PxSystem::setupNeighbors(const IntVector& numPx, PxSubj& subj) {
        global.neighbors.resize(subj.size());
        assert(numPx.size() == global.nlabels);

        // construct delaunay triangulations
        // implicitly assume that the index of numPx represents the label index
        for (int i = 0; i < numPx.size(); i++) {
            ParticleMesh mesh;
            mesh.constructNeighbors(i, numPx[i], subj, global.cutoffParams[i], global.neighbors);
        }
    }


    void PxSystem::clearForces() {
        for (int i = 0; i < subjs.size(); i++) {
            std::fill(subjs[i].forces.begin(), subjs[i].forces.end(), 0);
        }
    }

    // load data for particle system
    void PxSystem::loadSystem(ConfigFile& config) {
        global.load(config);

        sampler.setGlobalConfig(&global);
        subjs.resize(global.nsubjs);
        Setting& subjconfig = config["particles.subjects"];
        for (int i = 0; i < global.nsubjs; i++) {
            subjs[i].setGlobalConfig(&global);
            Setting& subjdata = subjconfig[i];
            subjs[i].regions.resize(global.nlabels);
            for (int j = 0; j < global.nlabels; j++) {
                subjs[i].regions[j].load(subjdata["labels"][j]);
            }
        }
    }


    // iterate subjects to load particle data
    // only when ignore-particle-input = false
    bool PxSystem::loadParticles(ConfigFile& config) {
        if (config["particles.ignore-particle-input"]) {
            return false;
        }

        Setting& subjconfig = config["particles.subjects"];

        bool ok = true;
        for (int i = 0; i < subjconfig.getLength() && ok; i++) {
            string f = subjconfig[i]["particle-input"];
            ok = subjs[i].load(f);
        }

        return ok;
    }

    // initialize sampler on first run
    bool PxSystem::loadSampler(ConfigFile& config) {
        // sampler needs to load region information
        // if sampler-input doesn't exist!!
        if (!config["particles.ignore-sampler-input"]) {
            string samplerCache = config["particles.sampler-cache"];
            if (sys.checkFile(samplerCache)) {
                if (sampler.load(samplerCache)) {
                    return true;
                }
            }
        }

        // main configuration setting
        Setting& mainConfig = config["particles"];

        // load each label information and create intersection
        Setting& regionList = config["particles.sampler.labels"];
        if (global.nlabels != regionList.getLength()) {
            cout << "sampler has different number of labels" << endl;
            exit(0);
        }

        /// global configuration setting
        sampler.setGlobalConfig(&global);

        // sampler region load
        sampler.regions.resize(global.nlabels);
        for (int i = 0; i < regionList.getLength(); i++) {
            // if there exists intersection cache
            if (!sampler.regions[i].load(regionList[i], false)) {
                // allocate new image buffer
                LabelImage::Pointer intersection = labelIO.NewImage(subjs[0].regions[i].labelmap);

                int nPixels = intersection->GetPixelContainer()->Size();
                LabelImage::PixelType* outputBuff = intersection->GetBufferPointer();

                // initialize label pointers for fast access
                std::vector<LabelImage::PixelType*> ptrs;
                ptrs.resize(global.nsubjs);

                for (int j = 0; j < global.nsubjs; j++) {
                    ptrs[j] = subjs[j].regions[i].labelmap->GetBufferPointer();
                }

                // loop over pixels to check if the pixel is intersection
                for (int ii = 0; ii < nPixels; ii++) {
                    // test if every labels have label
                    bool all = true;
                    for (unsigned int j = 0; all && j < ptrs.size(); j++) {
                        all = all && (*ptrs[j] > 0);
                    }
                    if (all) {
                        *outputBuff = 1;
                    }

                    // increment pixel pointer
                    for (unsigned int j = 0; j < ptrs.size(); j++) {
                        ptrs[j] ++;
                    }
                    outputBuff ++;
                }
                sampler.regions[i].labelmap = intersection;
                sampler.regions[i].deriveImages(regionList[i], intersection, true);
            }
        }

        // sample particles with no cache
        sampler.sampleParticles(global.numParticles);

        // save cache
        string file = "";
        if (mainConfig.lookupValue("sampler-initial-cache", file)) {
            ofstream of(file.c_str());
            sampler.save(of);
            of.close();
        }
        return false;
    }

    // iterate subjects to save particle data
    bool PxSystem::saveParticles(ConfigFile& config, string outputName) {
        Setting& subjconfig = config["particles.subjects"];

        bool ok = true;
        for (int i = 0; i < global.nsubjs && ok; i++) {
            string f = subjconfig[i][outputName];
            ofstream of(f.c_str());
            subjs[i].save(of);
            of.close();
        }
        return ok;
    }

    // duplicate particles from sampler to each subject
    void PxSystem::transferParticles() {
        for (int i = 0; i < global.nsubjs; i++) {
            subjs[i].setGlobalConfig(&global);
            subjs[i].resize(sampler.size());
            subjs[i].attrs = sampler.attrs;
            subjs[i].particles = sampler.particles;
        }
    }

    /**
     * 1) load configuration
     * 2) load particle data
     * 3) load sampler and run preprocessing if necessary
     * 4) save initialized particles
     */
    void PxSystem::initialize(Options& opts, StringVector& args) {
        _config.load(opts.GetConfigFile());
        loadSystem(_config);
        if (!loadParticles(_config)) {
            if (!loadSampler(_config)) {
                initialLoop();
                string samplerCache = _config["particles.sampler-cache"];
                ofstream of(samplerCache.c_str());
                sampler.save(of);
                of.close();
            }
            transferParticles();
            saveParticles(_config, "particle-input");
        }
    }

    // initial loop to distribute particles inside region
    void PxSystem::initialLoop() {
        double t0 = _config["particles.sampler-time-steps.[0]"];
        double dt = _config["particles.sampler-time-steps.[1]"];
        double t1 = _config["particles.sampler-time-steps.[2]"];

        ImageIO<LabelImage3> io;
        LabelImage::Pointer refImage = sampler.regions[0].labelmap;
        LabelImage3::Pointer tracker = createImage3(refImage, (t1 - t0) / dt + 1);

        int m = 0;
        for (double t = t0; t <= t1; t += dt, m++) {
            cout << "t = " << t << endl;
            sampler.clearForce();
            sampler.constrainParticles();

            // how to set repulsion paramter per region?
            sampler.computeRepulsion(true);
            sampler.constrainForces();
            sampler.updateSystem(dt);

            for (int i = 0; i < sampler.particles.size(); i++) {
                LabelImage::IndexType idx = sampler.getIndex(i);
                LabelImage3::IndexType tx;
                fordim (k) {
                    tx[k] = idx[k];
                }
                tx[2] = m;
                if (tracker->GetBufferedRegion().IsInside(tx)) {
                    tracker->SetPixel(tx, 1);
                }
            }
        }

//        printAdjacencyMatrix();

        io.WriteImage("initialLoop.nrrd", tracker);
    }


    /**
     * registration loop
     *
     */
    void PxSystem::loop() {
        double t0 = _config["particles.time-steps.[0]"];
        double dt = _config["particles.time-steps.[1]"];
        double t1 = _config["particles.time-steps.[2]"];

        ensemble.global = &global;

        ImageIO<LabelImage3> io;
        LabelImage::Pointer refImage = subjs[0].regions[0].labelmap;
        std::vector<LabelImage3::Pointer> trackers;

        for (int i = 0; i < global.nsubjs; i++) {
            LabelImage3::Pointer tracker = createImage3(refImage, (t1 - t0) / dt + 1);
            trackers.push_back(tracker);
        }


        setupNeighbors(global.numParticles, subjs[0]);

        int m = 0;
        int s = 0;
        for (double t = t0; t <= t1; t += dt, m++) {
            cout << "t = " << t << endl;

            if (m % 10 == 0) {
                // construct neighbors
                setupNeighbors(global.numParticles, subjs[++s%2]);
            }

            // compute internal forces
            for (int i = 0; i < global.nsubjs; i++) {
                subjs[i].clearForce();
                subjs[i].constrainParticles();
                // how to set repulsion paramter per region?
                subjs[i].computeRepulsion();
            }

            // apply least-square based affine transform
            affineTransformParticles();

            // now compute ensemble and intensity forces
            ensemble.computeAttraction(subjs);
//            cout << subjs[0].ensembleForces << endl;

            // update system
            for (int i = 0; i < global.nsubjs; i++ ) {
                subjs[i].constrainForces();
                subjs[i].updateSystem(dt);
                subjs[i].constrainParticles();
            }



            // mark each particle location at a tracker image
            for (int i = 0; i < global.nsubjs; i++) {
                for (int j = 0; j < subjs[0].particles.size(); j++) {
                    LabelImage::IndexType idx = subjs[i].getIndex(j);
                    LabelImage3::IndexType tx;
                    fordim (k) {
                        tx[k] = idx[k];
                    }
                    tx[2] = m;
                    if (trackers[i]->GetBufferedRegion().IsInside(tx)) {
                        trackers[i]->SetPixel(tx, 1);
                    }
                }
            }
        }


        for (int i = 0; i < global.nsubjs; i++) {
            string file = _config["particles.particle-images"][i];
            io.WriteImage(file, trackers[i]);
        }
    }


    void PxSystem::affineTransformParticles() {
        // copy from particle space to affineAligned space
        for (int i = 0; i < global.nsubjs; i++) {
            std::copy(subjs[i].particles.begin(), subjs[i].particles.end(), subjs[i].affineAligned.begin());
        }

        subjs[1].affineTxf.estimateAffineTransform(&subjs[0]);
    }

    /**
     * warp label images in according to its setting
     * setting = [ srcIdx, dstIdx, inputFile, outputFile ]
     * control point spacing and other bspline parameters may be cached to the system
     */
    void PxSystem::warpLabels(libconfig::Setting& warpedLabels) {
        for (int i = 0; i < warpedLabels.getLength(); i++) {
            int src = warpedLabels[i][0];
            int dst = warpedLabels[i][1];
            string input = warpedLabels[i][2];
            string output = warpedLabels[i][3];

            // if src-index and dst-index are not correct
            if (src < 0 || src >= global.nsubjs) {
                return;
            }
            if (dst < 0 || dst >= global.nsubjs) {
                return;
            }

            cout << "Warping: " << src << " => " << dst << " : " << input << " => " << output << endl;
            LabelImage::Pointer inputImage = labelIO.ReadCastedImage(input);
            ParticleWarp warp;
            warp.setParameters(_config);
            warp.reference = inputImage;
            warp.estimateBsplineWarp(subjs[src].particles, subjs[dst].particles);
            LabelImage::Pointer outputImage = warp.warpLabel(inputImage);
            labelIO.WriteImage(output, outputImage);
        }
    }

    /**
     * warp intensity images in according to its setting
     * setting = [ srcIdx, dstIdx, inputFile, outputFile ]
     * control point spacing and other bspline parameters may be cached to the system
     */
    void PxSystem::warpImages(libconfig::Setting& data) {
        for (int i = 0; i < data.getLength(); i++) {
            int src = data[i][0];
            int dst = data[i][1];
            string input = data[i][2];
            string output = data[i][3];

            // if src-index and dst-index are not correct
            if (src < 0 || src >= global.nsubjs) {
                return;
            }
            if (dst < 0 || dst >= global.nsubjs) {
                return;
            }

            // warp output
            cout << "Warping: " << src << " => " << dst << " : " << input << " => " << output << endl;
            RealImage::Pointer inputImage = io.ReadCastedImage(input);
            ParticleWarp warp;
            warp.reference = subjs[0].regions[0].labelmap;
            warp.estimateBsplineWarp(subjs[dst].particles, subjs[src].particles);
            RealImage::Pointer outputImage = warp.warpImage(inputImage);
            io.WriteImage(output, outputImage);
        }
    }

    void PxSystem::main(pi::Options &opts, StringVector &args) {
        initialize(opts, args);
        loop();
        saveParticles(_config, "particle-output");

        warpLabels(_config["particles/warped-labels"]);
        warpImages(_config["particles/warped-images"]);

        print();
        saveAdjacencyMatrix("adj.txt");
    }

    void PxSystem::print() {
        for (int i = 0; i < sampler.particles.size(); i++) {
            cout << sampler.particles[i] << endl;
        }
    }

    void PxSystem::saveAdjacencyMatrix(std::string file) {
        ofstream of(file.c_str());

        vnl_matrix<int> adjmat(global.totalParticles, global.totalParticles);
        adjmat.fill(0);

        for (int i = 0; i < global.neighbors.size(); i++) {
            for (int j = 0; j < global.neighbors[i].size(); j++) {
                adjmat[i][ global.neighbors[i][j] ] = 1;
            }
        }
        of << adjmat;
        of.close();
    }

#pragma mark ParticleRunner implementations
    void ParticleRunner::main(pi::Options &opts, StringVector &args) {
        try {
            _system.main(opts, args);
        } catch (ParseException & ex) {
            cout << "File: " << ex.getFile() << endl;
            cout << "Line: " << ex.getLine() << endl;
            cout << "Error: " << ex.getError() << endl;
        }
    }

    void ParticleRunner::print() {
    }


#pragma mark DemonsRunner
    void executeDemonsRunner(pi::Options &opts, StringVector &args) {
        DemonsRunner runner(opts, args);
        if (args.size() > 0) {
            if (args[0] == "optical-flow-mapping") {
                runner.computeOpticalFlowMapping();
            } else if (args[0] == "patch-mapping") {
                runner.computePatchMapping();
            }
        }
    }



    DemonsRunner::DemonsRunner(pi::Options &opts, StringVector &args) {
        _config.load("config.txt");
        if (_config.exists("build-patch")) {
            buildPatches(_config["build-patch"]);
        }
        if (_config.exists("patch-images")) {
            _config.readImages<PatchImage>(__patchImages, "patch-images");
        }
    }

    void DemonsRunner::computePatchMapping() {
        if (!_config.exists("dense-patch-mapping")) {
            return;
        }

        Setting& config = _config["dense-patch-mapping"];
        int fixedImageIdx = config["fixed-image-idx"];
        int movingImageIdx = config["moving-image-idx"];
        PatchImage::Pointer fixedImage = __patchImages[fixedImageIdx];
        PatchImage::Pointer movingImage = __patchImages[movingImageIdx];

        PatchImage::RegionType sourceRegion = fixedImage->GetBufferedRegion();
        PatchImage::RegionType activeRegion = _config.offsetRegion(sourceRegion, "dense-patch-mapping.region-offset");

        PatchCompare patchMaker;
        DisplacementFieldType::Pointer deformationField = patchMaker.performDenseMapping(fixedImage, movingImage, activeRegion);

        // deformation field output
        ImageIO<DisplacementFieldType> io;
        string deformationFieldFile = config["displacement-field-output"];
        io.WriteImage(deformationFieldFile, deformationField);

        // warped image output
        string warpedImage = config["warped-image-output"];
        io.WriteImageS<RealImage>(warpedImage, deformImage(_config.image(movingImageIdx), deformationField, _config.image(fixedImageIdx)));
    }

    void DemonsRunner::computeOpticalFlowMapping() {
        Setting& files = _config["optical-flow-mapping/files"];
        Setting& param = _config["optical-flow-mapping/params"];

        double dt = 0.1;
        param.lookupValue("timestep", dt);

        int iter = 1;
        param.lookupValue("iteration", iter);

        double fieldSigma = -1;
        param.lookupValue("field-sigma", fieldSigma);

        double velocitySigma = -1;
        param.lookupValue("velocity-sigma", velocitySigma);

        bool useDemonsFlow = false;
        param.lookupValue("demons-flow", useDemonsFlow);

        bool iterativeResampling = false;
        param.lookupValue("iterative-resampling", iterativeResampling);

        cout << "demons = " << useDemonsFlow << endl;
        cout << "iterative-resampling = " << iterativeResampling << endl;
        cout << "dt = " << dt << endl;
        cout << "iter  = " << iter << endl;
        cout << "field-sigma = " << fieldSigma << endl;
        cout << "velocity-sigma = " << velocitySigma << endl;

        PatchCompare patchTool;
        for (int i = 0; i < files.getLength(); i++) {
            string source = files[i][0];
            string target = files[i][1];
            string flowoutput = files[i][2];
            string warpoutput = files[i][3];

            cout << source << " => " << target << "; " << flowoutput << ", " << warpoutput << endl;

            RealImage::Pointer sourceImage = io.ReadCastedImage(source);
            RealImage::Pointer targetImage = io.ReadCastedImage(target);
            RealImage::Pointer initialImage = io.CopyImage(sourceImage);

            const int nPixels = sourceImage->GetPixelContainer()->Size();

            DisplacementFieldType::Pointer velocityImage;
            DisplacementFieldType::Pointer flowImage;

            ImageIO<DisplacementFieldType> flowIO;
            flowImage = flowIO.NewImageS<RealImage>(sourceImage);
            DisplacementFieldType::PixelType zeroFlow;
            zeroFlow.Fill(0);
            flowImage->FillBuffer(zeroFlow);

            for (int j = 0; j < iter; j++) {
                if (useDemonsFlow) {
                    velocityImage = patchTool.computeDemonsFlow(targetImage, sourceImage, dt);
                } else {
                    velocityImage = patchTool.computeOpticalFlow(targetImage, sourceImage, dt);
                }

                if (velocitySigma > 0) {
                    cout << "apply gaussian on velocity iter: " << j << endl;
                    velocityImage = applyGaussian<DisplacementFieldType>(velocityImage, velocitySigma);
                }

                if (iterativeResampling) {
                    sourceImage = deformImage(sourceImage, velocityImage, targetImage);
                } else {
                    // compute approximated update field
                    DisplacementFieldType::Pointer updateField = resampleField(flowImage, velocityImage);
                    DisplacementFieldType::PixelType* pFlow = flowImage->GetBufferPointer();
                    DisplacementFieldType::PixelType* pUpdateField = updateField->GetBufferPointer();

                    for (int k = 0; k < nPixels; k++) {
                        fordim (l) {
                            pFlow[k][l] += pUpdateField[k][l];
                        }
                    }
                    cout << endl;

                    if (fieldSigma > 0) {
                        cout << "apply gaussian on displacement iter: " << j << endl;
                        flowImage = applyGaussian<DisplacementFieldType>(flowImage, fieldSigma);
                    }
                    sourceImage = deformImage(initialImage, flowImage, targetImage);
                }

//                io.WriteImageS<DisplacementFieldType>("velocity.nrrd", velocityImage);
//                io.WriteImageS<DisplacementFieldType>("update.nrrd", updateField);

            }


            io.WriteImage(warpoutput, sourceImage);
            io.WriteImageS<DisplacementFieldType>(flowoutput, flowImage);
        }
    }

    RealImage::Pointer DemonsRunner::deformImage(RealImage::Pointer input, DisplacementFieldType::Pointer displacement, RealImage::Pointer refImage) {
        FieldTransformType::Pointer transform = FieldTransformType::New();
        transform->SetDisplacementField(displacement);

        ResampleImageFilterType::Pointer resampler = ResampleImageFilterType::New();
        resampler->SetInput(input);
        resampler->SetUseReferenceImage(true);
        resampler->SetReferenceImage(refImage);
        resampler->SetTransform(transform);
        resampler->Update();
        RealImage::Pointer warpedImage = resampler->GetOutput();
        warpedImage->DisconnectPipeline();
        return warpedImage;
    }

    // resample displaecement field
    DisplacementFieldType::Pointer DemonsRunner::resampleField(DisplacementFieldType::Pointer currentField, DisplacementFieldType::Pointer resamplingField) {
        typedef itk::VectorLinearInterpolateImageFunction<DisplacementFieldType> InterpolatorType;
        InterpolatorType::Pointer interpolator = InterpolatorType::New();

        FieldTransformType::Pointer transform = FieldTransformType::New();
        transform->SetDisplacementField(currentField);
        transform->SetInterpolator(interpolator);

        typedef itk::VectorResampleImageFilter<DisplacementFieldType, DisplacementFieldType> ResampleFilter;
        ResampleFilter::Pointer resampler = ResampleFilter::New();

        resampler->SetInput(resamplingField);
        resampler->SetSize(currentField->GetBufferedRegion().GetSize());
        resampler->SetOutputOrigin(currentField->GetOrigin());
        resampler->SetOutputSpacing(currentField->GetSpacing());
        resampler->SetOutputDirection(currentField->GetDirection());
        resampler->SetTransform(transform);

        resampler->Update();

        DisplacementFieldType::Pointer resampledField = resampler->GetOutput();
        resampledField->DisconnectPipeline();
        return resampledField;
    }




    void DemonsRunner::buildPatches(libconfig::Setting &setting) {
        bool checkFiles = setting["check-files"];
        PatchCompare patchMaker;
        string inputImageTag = setting["input"];
        string patchImageTag = "patch-images";
        for (int i = 0; i < _config[patchImageTag].getLength(); i++) {
            string output = _config[patchImageTag][i];
            if (!checkFiles || !io.FileExists(output.c_str())) {
                string input = _config[inputImageTag][i];
                RealImage::Pointer inputImage = io.ReadCastedImage(input);
                PatchImage::Pointer patchImage = patchMaker.buildPatchImage(inputImage);
                io.WriteImageS<PatchImage>(output, patchImage);
            }
        }
    }

}
