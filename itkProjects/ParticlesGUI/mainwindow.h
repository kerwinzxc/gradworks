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

class vtkPolyData;
class vtkGenericOpenGLRenderWindow;
class QVTKInteractor;

class MainWindow: public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = NULL);
    ~MainWindow();

    void ReadyToExperiments();
    
public slots:
    void on_actionOpenSource_triggered();
    void on_actionOpenSourceLabel_triggered();
    void on_actionOpenTarget_triggered();
    void on_actionOpenSurface_triggered();
    void on_actionRunImageParticles_triggered();
    void on_actionAnimation_triggered();
    void on_actionRandomParticlesInit_triggered();
    
    void updateScene();
    void chooseSlice();
    void on_animationTimeout();

private:
//    void AddPolyData(std::string name, vtkPolyData* poly, float r = 1, float g = 0, float b = 0, float opacity = 1.0);
//    void RemovePolyData(std::string name);
//    void RemoveAllPolyData();
    void LoadImage(QString fileName, int idx);
    void LoadLabel(QString fileName, int idx);

    inline bool IsSourceAvailable() { return m_ImageList.size() > 0 && m_ImageList[0].IsNotNull(); }
    inline bool IsTargetAvailable() { return m_ImageList.size() > 1 && m_ImageList[1].IsNotNull(); }
    inline bool IsImageAvailable(int n) { return m_ImageList.size() > n && m_ImageList[n]->HasImage(); }
    inline bool IsLabelAvailable(int n) { return m_ImageList.size() > n && m_ImageList[n]->HasLabel(); }
    inline int GetCurrentImage() { return ui.showSource->isChecked() ? 0 : 1; }
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

};

#endif
