// mlvg test application
// Copyright (C) 2019-2022 Madrona Labs LLC
// This software is provided 'as-is', without any express or implied warranty.
// See LICENSE.txt for details.

#include "testAppView.h"

#include "madronalib.h"

#include "MLDialBasic.h"
#include "MLResizer.h"
#include "MLTextLabelBasic.h"
#include "MLSVGImage.h"
#include "MLSVGButton.h"

#include "MLParameters.h"
#include "MLSerialization.h"

#include "../build/resources/testapp/resources.c"

constexpr float textSizeFix{0.85f};
ml::Rect largeDialRect{0, 0, 1.5, 1.5};


TestAppView::TestAppView(Rect size, Path controllerName) :
_controllerName(controllerName)
{
  float largeDialSize{0.55f};

  // initialize drawing properties before controls are made
  _drawingProperties.setProperty("mark", colorToMatrix({0.01, 0.01, 0.01, 1.0}));
  _drawingProperties.setProperty("background", colorToMatrix({0.8, 0.8, 0.8, 1.0}));
  _drawingProperties.setProperty("draw_background_grid", true);
  _defaultSystemSize = Vec2(size.width(), size.height());
    
  // add labels to background
  textUtils::NameMaker WidgetNamer;
  auto addControlLabel = [&](TextFragment t, float width, Vec2 center)
  {
    _view->_backgroundWidgets.add_unique< TextLabelBasic >(WidgetNamer.nextName(), WithValues{
      { "bounds", rectToMatrix( alignCenterToPoint(ml::Rect{0, 0, width, 0.25}, center + Vec2{0., 0.01}))},
      { "h_align", "center" },
      { "v_align", "middle" },
      { "text", t },
      { "font", "madronasansoblique" },
      { "text_size", 0.30*textSizeFix },
      { "text_spacing", 0.0f }
    } );
  };
  
  addControlLabel("size", 0.5, {1.875, 2.5});
  addControlLabel("decay", 0.75, {3.625, 2.5});
  addControlLabel("tone", 0.5, {5.375, 2.5});

  constexpr float bigDialsCenterY{2.0f};
  
  _view->_widgets.add_unique< DialBasic >("freq1", WithValues{
    {"bounds", rectToMatrix(alignCenterToPoint(largeDialRect, {2.0, bigDialsCenterY})) },
    {"size", largeDialSize },
    {"param", "freq1" }
  } );
  
  _view->_widgets.add_unique< DialBasic >("freq2", WithValues{
    {"bounds", rectToMatrix(alignCenterToPoint(largeDialRect, {4.0, bigDialsCenterY})) },
    {"size", largeDialSize },
    {"param", "freq2" }
  } );
  
  _view->_widgets.add_unique< DialBasic >("gain", WithValues{
    {"bounds", rectToMatrix(alignCenterToPoint(largeDialRect, {6.0, bigDialsCenterY})) },
    {"size", largeDialSize },
    {"param", "gain" }
  } );
  
  _initializeParams();

  // grid size of entire interface, for background and other drawing
  _view->setProperty("grid_units_x", kGridUnitsX);
  _view->setProperty("grid_units_y", kGridUnitsY);
  
  forEach< Widget >
  (_view->_widgets, [&](Widget& w)
   {
    w.setProperty("visible", true);
  }
   );
  _view->setDirty(true);
  
  _previousFrameTime = system_clock::now();
  
  _ioTimer.start([=](){ _handleGUIEvents(); }, milliseconds(1000/60));
  
  Actor::start();
  
  _debugTimer.start([=]() { debug(); }, milliseconds(1000));
}


void TestAppView::layoutView()
{
  int gx = _view->getIntProperty("grid_units_x");
  int gy = _view->getIntProperty("grid_units_y");
  
  //_view->_widgets["size"]->setRectProperty("bounds", alignCenterToPoint(largeDialRect, {6.0, gy - 1.0f}));
}


TestAppView::~TestAppView ()
{
  _ioTimer.stop();
  Actor::stop();
  removeActor(this);
}

int TestAppView::getElapsedTime()
{
  // return time elapsed since last render
  time_point<system_clock> now = system_clock::now();
  auto elapsedTime = duration_cast<milliseconds>(now - _previousFrameTime).count();
  _previousFrameTime = now;
  return elapsedTime;
}


#pragma mark from ml::AppView

void TestAppView::initializeResources(NativeDrawContext* nvg)
{
  if (nvg)
  {
    // fonts
    int font1 = nvgCreateFontMem(nvg, "MLVG_sans", (unsigned char*)resources::D_DIN_otf, resources::D_DIN_otf_size, 0);
    const unsigned char* pFont1 = reinterpret_cast<const unsigned char *>(&font1);
    _resources["madronasans"] = ml::make_unique< Resource >(pFont1, pFont1 + sizeof(int));

    int font2 = nvgCreateFontMem(nvg, "MLVG_italic", (unsigned char *)resources::D_DIN_Italic_otf, resources::D_DIN_Italic_otf_size, 0);
    const unsigned char* pFont2 = reinterpret_cast<const unsigned char *>(&font2);
    _resources["madronasansoblique"] = ml::make_unique< Resource >(pFont2, pFont2 + sizeof(int));
    
    // raster images
    int flags = 0;
    int img1 = nvgCreateImageMem(nvg, flags, (unsigned char *)resources::vignette_jpg, resources::vignette_jpg_size);
    const unsigned char* pImg1 = reinterpret_cast<const unsigned char *>(&img1);
    //_resources["background"] = ml::make_unique< Resource >(pImg1, pImg1 + sizeof(int));
    
    // SVG images    
    ml::AppView::createVectorImage("tesseract", resources::Tesseract_Mark_svg, resources::Tesseract_Mark_svg_size);
  }
}

void TestAppView::_initializeParams()
{
  // read parameter descriptions from processorParameters.h
  ParameterDescriptionList pdl;
  readParameterDescriptions(pdl);
  
  // TODO can we share the Controller's parameter tree?
  buildParameterTree(pdl, _params);
  
  // build index of widgets by parameter.
  // for each parameter, collect Widgets responding to it
  // and add the parameter to the Widget's param tree.
  for(auto& paramDesc : pdl)
  {
    Path paramName = paramDesc->getTextProperty("name");
    
    forEach< Widget >
    (_view->_widgets, [&](Widget& w)
     {
      if(w.knowsParam(paramName))
      {
        _widgetsByParameter[paramName].push_back(&w);
        w.setParameterDescription(paramName, *paramDesc);
      }
    }
     );
  }
  
  // build index of any widgets that refer to collections.
  forEach< Widget >
  (_view->_widgets, [&](Widget& w)
   {
    if(w.hasProperty("collection"))
    {
      const Path collName(w.getTextProperty("collection"));
      _widgetsByCollection[collName].push_back(&w);
    }
  });
  
  // build index of any widgets that need signals, and subscribe to those signals.
  for(auto& w : _view->_widgets)
  {
    if(w->hasProperty("signal_name"))
    {
      const Path sigName(w->getTextProperty("signal_name"));
      _widgetsBySignal[sigName].push_back(w.get());

      // message the controller to subscribe to the signal.
      sendMessageToActor(_controllerName, Message{"do/subscribe_to_signal", pathToText(sigName)});
    }
  }
  
  // give each Widget a chance to do setup now: after it has its
  // parameter description(s) and before it is animated or drawn.
  for(auto& w : _view->_widgets)
  {
    w->setupParams();
  }
}


// called when native view size changes.
// width and height are the size of the view in system coordinates.
void TestAppView::viewResized(NativeDrawContext* nvg, int width, int height)
{
  _view->setDirty(true);
  
  // get dims in UI coordinates (pixels)
  float displayScale = _GUICoordinates.displayScale;
  int nativeWidth = width*displayScale;
  int nativeHeight = height*displayScale;
  
  int gridSize;
  int gridUnitsX{int(kGridUnitsX)};
  int gridUnitsY{int(kGridUnitsY)};

  if(kFixedRatioSize)
  {
    // scale both the app and the grid,
    // leave a border around the content area so that it is always
    // an even multiple of the grid size in native pixels.
    int ux = nativeWidth/kGridUnitsX;
    int uy = nativeHeight/kGridUnitsY;
    gridSize = std::min(ux, uy);
    float contentWidth = gridSize*kGridUnitsX;
    float contentHeight = gridSize*kGridUnitsY;
    float borderX = (nativeWidth - contentWidth)/2;
    float borderY = (nativeHeight - contentHeight)/2;
    _borderRect = ml::Rect{borderX, borderY, contentWidth, contentHeight};
  }
  else
  {
    // TODO user-adjustable grid size? only for subviews?
    gridSize = kDefaultGridSize;
    gridUnitsX = nativeWidth/gridSize;
    gridUnitsY = nativeHeight/gridSize;
    _view->setProperty("grid_units_x", gridUnitsX);
    _view->setProperty("grid_units_y", gridUnitsY);

    float contentWidth = gridSize*gridUnitsX;
    float contentHeight = gridSize*gridUnitsY;
    float borderX = (nativeWidth - contentWidth)/2;
    float borderY = (nativeHeight - contentHeight)/2;
    _borderRect = ml::Rect{borderX, borderY, contentWidth, contentHeight};
 //   _borderRect = ml::Rect(0, 0, nativeWidth, nativeHeight);
  }
  
  // set new coordinate transform values for GUI renderer - displayScale remains the same
  // width and height are in pixel coordinates
  // MLTEST _GUICoordinates.pixelSize = Vec2(nativeWidth, nativeHeight);
  _GUICoordinates.gridSize = gridSize;
  _GUICoordinates.origin = getTopLeft(_borderRect);
  
  // set bounds for top-level View in grid coordinates
  {
    ml::Rect viewBounds {0, 0, float(gridUnitsX), float(gridUnitsY)};
    setBoundsRect(*_view, viewBounds);
  }
  
  // fixed size widgets are measured in pixels, so they need their grid-based sizes recalculated
  // when the view dims change.
  forEach< Widget >
  (_view->_widgets, [&](Widget& w)
   {
    if(w.getProperty("fixed_size"))
    {
      // get anchor point for widget in system coords from anchor param on (0, 1)
      Vec2 systemAnchor = matrixToVec2(w.getProperty("anchor").getMatrixValue());
      systemAnchor = systemAnchor * Vec2(width, height);
      
      // fixed widget bounds are in system coords (for same apparent size)
      Vec4 systemBounds = w.getRectProperty("fixed_bounds");
      
      //Vec4 viewBounds = _GUICoordinates.systemToNative(systemBounds);
      systemBounds = translate(systemBounds, systemAnchor);
      ml::Rect gridBounds = _GUICoordinates.systemToGrid(systemBounds);
      w.setProperty("bounds", rectToMatrix(gridBounds));
    }
  }
   );
  layoutView();
}

void TestAppView::renderView(NativeDrawContext* nvg, Layer* backingLayer)
{
  if(!backingLayer) return;
  int w = backingLayer->width;
  int h = backingLayer->height;
  
  // TODO move resource types into Renderer, DrawContext points to Renderer
  DrawContext dc{nvg, &_resources, &_drawingProperties, &_vectorImages, _GUICoordinates};
  
  // first, allow Widgets to draw any needed animations outside of main nvgBeginFrame().
  // Do animations and handle any resulting messages immediately.
  MessageList ml = _view->animate(getElapsedTime(), dc);
  enqueueMessageList(ml);
  handleMessagesInQueue();
  
  // begin the frame on the backing layer
  drawToLayer(backingLayer);
  nvgBeginFrame(nvg, w, h, 1.0f);
  
  // TODO lower level clear?
  
  // if top level view is dirty, clear entire window
  if(_view->isDirty())
  {
    nvgBeginPath(nvg);
    nvgRect(nvg, ml::Rect{-10000, -10000, 20000, 20000});
    auto bgColor = getColor(dc, "background");

    nvgFillColor(nvg, bgColor);
    nvgFill(nvg);
  }
  
  ml::Rect topViewBounds = dc.coords.gridToNative(getBounds(*_view));
  nvgIntersectScissor(nvg, topViewBounds);
  auto topLeft = getTopLeft(topViewBounds);
  nvgTranslate(nvg, topLeft);
  
  _view->draw(translate(dc, -topLeft));
  
  // end the frame.
  nvgEndFrame(nvg);
  _view->setDirty(false);
}

void TestAppView::doAttached (void* pParent, int flags)
{
  float w = _defaultSystemSize.x();
  float h = _defaultSystemSize.y();
  
  if(pParent != _parent)
  {
    _platformView = ml::make_unique< PlatformView >(pParent, Vec4(0, 0, w, h), this, _platformHandle, flags);
    _parent = pParent;
  }
  
  doResize(_constrainSize(Vec2(w, h)));
}

void TestAppView::pushEvent(GUIEvent g)
{
  _inputQueue.push(g);
}

// given a pointer to a size in system UI coordinates, tweak the size to be a valid window size.
Vec2 TestAppView::_constrainSize(Vec2 size)
{
  Vec2 newSize = size;
  if(kFixedRatioSize)
  {
    newSize = vmax(newSize, Vec2{kMinGridSize*kGridUnitsX, kMinGridSize*kGridUnitsY});
    newSize = vmin(newSize, Vec2{kMaxGridSize*kGridUnitsX, kMaxGridSize*kGridUnitsY});
  }
  return newSize;
}

// set new editor size in system coordinates.
void TestAppView::doResize(Vec2 newSize)
{
  int width = newSize[0];
  int height = newSize[1];
  float scale = _GUICoordinates.displayScale;
  Vec2 newViewSize = Vec2(width, height)*scale;
  
  if(_GUICoordinates.pixelSize != newViewSize)
  {
    _GUICoordinates.pixelSize = newViewSize;
    
    // resize our canvas, in system coordinates
    if(_platformView)
      _platformView->resizeView(width, height);
  }
}

// maintain states for click-and-hold timer
// param: event in native coordinates
GUIEvent TestAppView::detectDoubleClicks(GUIEvent e)
{
  constexpr int kDoubleClickRadius = 8;
  constexpr int kDoubleClickMs = 500;
  
  Vec2 systemPosition = _GUICoordinates.nativeToSystem(e.position);
  GUIEvent r = e;
  
  if(e.type == "up")
  {
    if(_doubleClickTimer.isActive())
    {
      if(magnitude(Vec2(systemPosition - _doubleClickStartPosition)) < kDoubleClickRadius)
      {
        r.keyFlags |= commandModifier;
        _doubleClickTimer.stop();
      }
    }
    else
    {
      _doubleClickStartPosition = systemPosition;
      
      // timer doesn't need to execute anything, just expire.
      _doubleClickTimer.callOnce( [](){}, milliseconds(kDoubleClickMs) );
    }
  }
  return r;
}

// Handle the queue of GUIEvents from the View by routing events to Widgets
// and handling any returned Messages.
void TestAppView::_handleGUIEvents()
{
  while (_inputQueue.elementsAvailable())
  {
    auto e = _inputQueue.pop();
    GUIEvent nativeEvent = detectDoubleClicks(e);
    GUIEvent gridEvent(nativeEvent);
    gridEvent.position = _GUICoordinates.nativeToGrid(gridEvent.position);

    
    // std::cout << "_dismissPopupOnClick: " << (dismissed ? "TRUE" : "FALSE") << "\n";
    
    // The top-level processGUIEvent call.
    // Send input events to all Widgets in our View and handle any resulting messages.
    enqueueMessageList(_view->processGUIEvent(_GUICoordinates, gridEvent));
    handleMessagesInQueue();
  }
}

void TestAppView::_sendParameterToWidgets(const Message& msg)
{
  if(msg.value)
  {
    // get param name
    Path pname = tail(msg.address);
    
    // send to Widgets that care about it
    for(auto pw : _widgetsByParameter[pname])
    {
      // if Widget is not engaged, send it the new value.
      if(!pw->engaged)
      {
        sendMessage(*pw, msg);
      }
    }
  }
}

void TestAppView::debug()
{
  //std::cout << "TestAppView: " << getMessagesAvailable() << " messages in queue. max: "
  //  << _maxQueueSize << " handled: " << _msgCounter << " \n";
  //_msgCounter = 0;
}


// Actor implementation

void TestAppView::onMessage(Message msg)
{
  std::cout << "TestAppView: onMessage: " << msg.address << " : " << msg.value << "\n";
  
  if(head(msg.address) == "editor")
  {
    // we are the editor, so remove "editor" and handle message
    msg.address = tail(msg.address);
  }
  
  switch(hash(head(msg.address)))
  {
    case(hash("set_param")):
    {
      switch(hash(head(tail(msg.address))))
      {
        default:
        {
          // no local parameter was found, set a plugin parameter
          
          // store param value in local tree.
          Path paramName = tail(msg.address);
          if(!paramName)
          {
            std::cout << "setting null param! \n";
          }
          
          _params.setParamFromNormalizedValue(paramName, msg.value);
          
          // if the parameter change message is not from the controller,
          // forward it to the controller.
          if(!(msg.flags & kMsgFromController))
          {
            sendMessageToActor(_controllerName, msg);
          }
          
          // if the message comes from a Widget, we do send the parameter back
          // to other Widgets so they can synchronize. It's up to individual
          // Widgets to filter out duplicate values.
          _sendParameterToWidgets(msg);
          break;
        }
      }
      break;
    }
      
    case(hash("do")):
    {
      break;
    }
    default:
    {
      // try to forward the message to another receiver
      switch(hash(head(msg.address)))
      {
        case(hash("controller")):
        {
          msg.address = tail(msg.address);
          sendMessageToActor(_controllerName, msg);
          break;
        }
        default:
        {
          // uncaught
          break;
        }
      }
      break;
    }
  }
}

