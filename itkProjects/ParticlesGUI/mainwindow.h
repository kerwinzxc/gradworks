#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGui/QMainWindow>
#include "ui_mainwindow.h"
#include <vtkRenderer.h>
#include <map>
#include <string>
#include <QList>
#include <QTimer>
#include <QActionGroup>
#include "myImageContainer.h"
#include "vtkPropScene.h"
#include "PropertyAccess.h"
#include "myEventCallback.h"
#include "itkOptimizerCommon.h"
#include "dialogPatchCompare.h"
#include "dialogBSplineVis.h"
#include "myImageParticlesAlgorithm.h"

class vtkPolyData;
class vtkGenericOpenGLRenderWindow;
class QVTKInteractor;
class ImageParticlesAlgorithm;

class MainWindow: public QMainWindow, public EventCallback {
    Q_OBJECT

    friend class PatchCompareDialog;
    friend class BSplineVisDialog;

public:
    MainWindow(QWidget* parent = NULL);
    ~MainWindow();

    ImageParticlesAlgorithm::Pointer GetImageParticlesAlgorithm();
    void ReadyToExperiments();
    virtual void EventRaised(int eventId, int eventCode, const void* src = 0, void* data = 0);
    
public slots:
    void on_actionOpenCompareWindow_triggered();
    void on_actionAddImage_triggered();
    void on_actionAddLabel_triggered();
    void on_actionOpenSurface_triggered();
    void on_actionSaveSlices_triggered();
    void on_actionSurfaceSmoothing_triggered();
    void on_actionSurfaceSmoothingContinue_triggered();
    void on_actionOpenBSplineVis_triggered();
    
    void on_actionTest_triggered();
    void on_actionRunImageParticles_triggered();
    void on_actionTPS_triggered();
    void on_actionEBS_triggered();
    void on_actionBSpline_triggered();
    void on_actionR2LogRTPS_triggered();
    void on_actionAnimation_triggered();
    void on_actionRandomParticlesInit_triggered();
    void on_actionContinue_triggered();
    void on_actionRunOnIntersection_triggered();
    void on_derivedImages_currentIndexChanged(int n);
    void on_graphicsView_mousePressed(QMouseEvent* event);
    void on_actionLoadParticleWorks_triggered();
    void on_actionLoadParticleWorksSingle_triggered();

    void on_actionSaveParticles_triggered();
    void on_actionLoadParticles_triggered();

    void selectImage(int);
    void selectLabel(int);
    void updateScene();
    void chooseSlice();
    void on_animationTimeout();

protected:
    virtual void showEvent(QShowEvent* event);
    
private:
//    void AddPolyData(std::string name, vtkPolyData* poly, float r = 1, float g = 0, float b = 0, float opacity = 1.0);
//    void RemovePolyData(std::string name);
//    void RemoveAllPolyData();
    void LoadImage(QString fileName);
    void LoadLabel(QString fileName);
    void LoadSurface(QString fileName);
    void LoadParticle(QString& fileName);

    inline bool IsSourceAvailable() { return m_ImageList.size() > 0 && m_ImageList[0].IsNotNull(); }
    inline bool IsTargetAvailable() { return m_ImageList.size() > 1 && m_ImageList[1].IsNotNull(); }
    inline bool IsImageAvailable(int n) { return m_ImageList.size() > n && m_ImageList[n]->HasImage(); }
    inline bool IsLabelAvailable(int n) { return m_ImageList.size() > n && m_ImageList[n]->HasLabel(); }
    inline int GetCurrentImage() { return ui.grayImages->currentIndex(); }
    inline int GetCurrentView() { return ui.showXY->isChecked() ? 0 : ui.showYZ->isChecked() ? 1 : 2; }

    Ui::MainWindow ui;
    vtkRenderer* m_Renderer;
    QVTKInteractor* m_Interactor;

    typedef std::map<std::string, vtkProp*> PropMapType;
    typedef std::pair<std::string, vtkProp*> NamedProp;
    PropMapType m_PropMap;

    vtkPropScene m_PropScene;
    QGraphicsScene m_scene;
    ImageContainer::List m_ImageList;
    QTimer m_Timer;
    QActionGroup m_ParticleColors;

    PropertyAccess m_Props;

    // dialog for image comparison
    PatchCompareDialog m_CompareDialog;

    BSplineVisDialog m_BSplineVisDialog;

};

#endif
