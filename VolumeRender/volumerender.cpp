#include "volumerender.h"

using namespace Web;

VolumeRender::VolumeRender(int size, float *data, WContainerWidget *parent):
    _RVESize(size),
    _ptrToRVEdata(data),
    WGLWidget(parent)
{
#ifdef USER_SIDE_CONTROL
    // see _initializeUserSideMouseControl()
#else
    mouseWentDown().connect(this,&VolumeRender::_onMouseWentDown);
    mouseDragged().connect(this,&VolumeRender::_onMouseDragged);
    mouseWheel().connect(this,&VolumeRender::_onMouseWheel);
#endif //USER_SIDE_CONTROL

}

void VolumeRender::_initShaders()
{
    /// see http://www3.cs.stonybrook.edu/~igutenko/webglvolrender.html

    // Draw into fragment buffer the box world-space position as RGB colors
    std::string vertexShaderBackfacesSrc = "\
        attribute   highp   vec3    aVrtPos;        \n\
        uniform     highp   mat4    uSceneMatrix;   \n\
        varying     highp   vec4    vPosition;      \n\
        void main(void)                             \n\
        {                                           \n\
            gl_Position =                           \n\
                uSceneMatrix * vec4(aVrtPos, 1.0);  \n\
            vPosition = vec4(aVrtPos, 1.0);         \n\
        }";

    std::string fragmentShaderBackfacesSrc = "\
        precision   highp   float;                              \n\
        varying     highp   vec4    vPosition;                  \n\
        void main(void)                                         \n\
        {                                                       \n\
            vec3 positionScaled = vPosition.xyz * 0.5 + 0.5;    \n\
            gl_FragColor = vec4(positionScaled, 1.0);           \n\
        }";

    std::string vertexShaderRaycastSrc = "\
        attribute   highp   vec3    aVrtPos;        \n\
        uniform     highp   mat4    uSceneMatrix;   \n\
        varying     highp   vec3    vTexCrd;        \n\
        void main(void)                             \n\
        {                                           \n\
            gl_Position =                           \n\
                uSceneMatrix * vec4(aVrtPos, 1.0);  \n\
            vTexCrd = aVrtPos.xyz * 0.5 + 0.5;      \n\
        }";

    std::string fragmentShaderRaycastSrc = "\
        precision   mediump float;                                  \n\
        uniform     highp   sampler2D   uBackfaceTextureSampler;    \n\
        uniform     highp   sampler2D   uVolumeTextureSampler;      \n\
        uniform     highp   float       uBackfaceTextureWidth;      \n\
        uniform     highp   float       uBackfaceTextureHeight;     \n\
        uniform     highp   float       uSize;                      \n\
        uniform     highp   float       uInnerBottomCutLevel;       \n\
        uniform     highp   float       uInnerTopCutLevel;          \n\
        varying     highp   vec3        vTexCrd;                    \n\
                                                                    \n\
        vec4 sampleAs3DTexture(vec3 pos)                            \n\
        {                                                           \n\
            vec2 tex;                                               \n\
            tex.x = pos.x;                                          \n\
            tex.y = pos.y/uSize+floor(pos.z*uSize)/uSize;           \n\
            return texture2D(uVolumeTextureSampler, tex);           \n\
        }                                                           \n\
                                                                    \n\
        vec4 grayToRainbow(vec4 col)                                \n\
        {                                                           \n\
            float inv = (1.0-col.r)*4.0;                            \n\
            float X = floor(inv);                                   \n\
            float Y = inv-X;                                        \n\
            if(X==0.0)      {col.r=1.0; col.g=Y; col.b=0.0;}        \n\
            else if(X==1.0) {col.r=1.0-Y; col.g=1.0; col.b=0.0;}    \n\
            else if(X==2.0) {col.r=0.0; col.g=1.0; col.b=Y;}        \n\
            else if(X==3.0) {col.r=0.0; col.g=1.0-Y; col.b=1.0;}    \n\
            else if(X==4.0) {col.r=0.0; col.g=0.0; col.b=1.0;}      \n\
            return col;                                             \n\
        }                                                           \n\
                                                                    \n\
        // WTF??? The 'break' statement doesn't work!               \n\
        void main(void)                                             \n\
        {                                                           \n\
            vec4 dst;                                               \n\
            vec3 rayEnd = texture2D(uBackfaceTextureSampler,        \n\
                    gl_FragCoord.xy / vec2(                         \n\
                        uBackfaceTextureWidth,                      \n\
                        uBackfaceTextureHeight)).rgb *              \n\
                2.0 - 1.0;                                          \n\
            vec3 rayStart = vTexCrd * 2.0 - 1.0;                    \n\
            vec3 dir = rayEnd.rgb - rayStart.rgb;                   \n\
            vec3 step = dir / 128.0;                                \n\
//            vec3 ray = rayStart;                                    \n\
            vec3 ray = rayEnd;                                      \n\
            for (int i=0; i<128; ++i)                               \n\
            {                                                       \n\
                vec4 src = sampleAs3DTexture(ray * 0.5 + 0.5);      \n\
                if(src.r >= uInnerBottomCutLevel && src.r <= uInnerTopCutLevel)dst = src;\n\
//                if(src.r > dst.r) dst = src;                        \n\
//                ray += step;                                        \n\
                ray -= step;                                        \n\
            }                                                       \n\
            gl_FragColor = dst;                                     \n\
//            gl_FragColor = grayToRainbow(dst);                      \n\
//            gl_FragColor = sampleAs3DTexture(vTexCrd);              \n\
        }";

    Shader vertexShader = createShader(VERTEX_SHADER);
    shaderSource(vertexShader, vertexShaderBackfacesSrc);
    compileShader(vertexShader);

    Shader fragmentShader = createShader(FRAGMENT_SHADER);
    shaderSource(fragmentShader, fragmentShaderBackfacesSrc);
    compileShader(fragmentShader);

    Shader vertexShaderSecond = createShader(VERTEX_SHADER);
    shaderSource(vertexShaderSecond, vertexShaderRaycastSrc);
    compileShader(vertexShaderSecond);

    Shader fragmentShaderSecond = createShader(FRAGMENT_SHADER);
    shaderSource(fragmentShaderSecond, fragmentShaderRaycastSrc);
    compileShader(fragmentShaderSecond);

    _shaderProgramFirst = createProgram();
    attachShader(_shaderProgramFirst, fragmentShader);
    attachShader(_shaderProgramFirst, vertexShader);
    linkProgram(_shaderProgramFirst);

    _shaderProgramSecond = createProgram();
    attachShader(_shaderProgramSecond, fragmentShaderSecond);
    attachShader(_shaderProgramSecond, vertexShaderSecond);
    linkProgram(_shaderProgramSecond);
}

void VolumeRender::_initBox()
{
    float dX = 1;
    float dY = 1;
    float dZ = 1;
    float _boxVertices[] =
    {
        -dX,-dY,-dZ,	-dX,-dY, dZ,	-dX, dY, dZ,	//1		- left
         dX, dY,-dZ,	-dX,-dY,-dZ,	-dX, dY,-dZ,	//2		- back
         dX,-dY, dZ,	-dX,-dY,-dZ,	 dX,-dY,-dZ,	//3		- bottom
         dX, dY,-dZ,	 dX,-dY,-dZ,	-dX,-dY,-dZ,	//4		- back
        -dX,-dY,-dZ,	-dX, dY, dZ,	-dX, dY,-dZ,	//5		- left
         dX,-dY, dZ,	-dX,-dY, dZ,	-dX,-dY,-dZ,	//6		- bottom
        -dX, dY, dZ,	-dX,-dY, dZ,	 dX,-dY, dZ,	//7		- front
         dX, dY, dZ,	 dX,-dY,-dZ,	 dX, dY,-dZ,	//8		- right
         dX,-dY,-dZ,	 dX, dY, dZ,	 dX,-dY, dZ,	//9		- right
         dX, dY, dZ,	 dX, dY,-dZ,	-dX, dY,-dZ,	//10	- top
         dX, dY, dZ,	-dX, dY,-dZ,	-dX, dY, dZ,	//11	- top
         dX, dY, dZ,	-dX, dY, dZ,	 dX,-dY, dZ 	//12	- front
    };
    _boxVerticesBuffer = createBuffer();
    bindBuffer(ARRAY_BUFFER, _boxVerticesBuffer);
    bufferDatafv<float*>(
                ARRAY_BUFFER,
                _boxVertices,
                _boxVertices + 108,
                STATIC_DRAW);
}

void VolumeRender::_initFBO()
{
    _framebuffer = createFramebuffer();
    bindFramebuffer(FRAMEBUFFER, _framebuffer);

    _framebufferTexture = createTexture();
    bindTexture(TEXTURE_2D, _framebufferTexture);
    texImage2D(
                TEXTURE_2D,
                0,
                RGBA,
                this->width().value(),
                this->height().value(),
                0,
                RGBA);
    texParameteri(TEXTURE_2D, TEXTURE_WRAP_S, CLAMP_TO_EDGE);
    texParameteri(TEXTURE_2D, TEXTURE_WRAP_T, CLAMP_TO_EDGE);
    texParameteri(TEXTURE_2D, TEXTURE_MAG_FILTER, NEAREST);
    texParameteri(TEXTURE_2D, TEXTURE_MIN_FILTER, NEAREST);
    bindTexture(TEXTURE_2D, "null");

    _renderbuffer = createRenderbuffer();
    bindRenderbuffer(RENDERBUFFER, _renderbuffer);
    renderbufferStorage(
                RENDERBUFFER,
                DEPTH_COMPONENT16,
                this->width().value(),
                this->height().value());
    bindRenderbuffer(RENDERBUFFER, "null");

    framebufferTexture2D(FRAMEBUFFER, COLOR_ATTACHMENT0, TEXTURE_2D, _framebufferTexture, 0);
    framebufferRenderbuffer(FRAMEBUFFER, DEPTH_ATTACHMENT, RENDERBUFFER, _renderbuffer);

    bindFramebuffer(FRAMEBUFFER, "null");
}

void VolumeRender::_preloadRVEIntoGrayscaleTexture()
{
    _RVEtexture = createTexture();
    bindTexture(TEXTURE_2D, _RVEtexture);
    {
        char buf[30];
        std::string _js;
        _js += "\nctx.texImage2D(";
        _js += "ctx.TEXTURE_2D,";
        _js += "0,";
        _js += "ctx.LUMINANCE,";
        _js += itoa(_RVESize, buf, 10);
        _js += ",";
        _js += itoa(_RVESize*_RVESize, buf, 10);
        _js += ",";
        _js += "0,";
        _js += "ctx.LUMINANCE,";
        _js += "ctx.UNSIGNED_BYTE,";
        _js += "new Uint8Array([";
        for(int i=0; i < _RVESize*_RVESize*_RVESize; ++i)
        {
            _js += (i == 0 ? "" : ",");
            _js += itoa((int)(_ptrToRVEdata[i]*255), buf, 10);
        }
        _js += "]));\n";
        injectJS(_js);
    }
    texParameteri(TEXTURE_2D, TEXTURE_WRAP_S, REPEAT);
    texParameteri(TEXTURE_2D, TEXTURE_WRAP_T, REPEAT);
    texParameteri(TEXTURE_2D, TEXTURE_MAG_FILTER, NEAREST);
    texParameteri(TEXTURE_2D, TEXTURE_MIN_FILTER, NEAREST);
    bindTexture(TEXTURE_2D, "null");
}

void VolumeRender::initializeGL()
{
    _initFBO();
    _initShaders();
    _preloadRVEIntoGrayscaleTexture();
    _initBox();

    enable(DEPTH_TEST);
    enable(CULL_FACE);
    enable(BLEND);
    blendEquation(FUNC_ADD);
    blendFunc(SRC_ALPHA ,ONE_MINUS_SRC_ALPHA);

    viewport(0, 0, (unsigned) this->width().value(), (unsigned) this->height().value());

#ifdef USER_SIDE_CONTROL
    _mModel     = createJavaScriptMatrix4();
    _mControl   = createJavaScriptMatrix4();
    _mWorld     = createJavaScriptMatrix4();
    _mProj      = createJavaScriptMatrix4();
    _mScene     = createJavaScriptMatrix4();

    WMatrix4x4 _m;
    _m.setToIdentity();
    _m.scale(0.5, 0.5, 0.5);
    setJavaScriptMatrix4(_mModel, _m);

    _m.setToIdentity();
    _m.lookAt(
                0, 0, 2,    // camera default position
                0, 0, 0,    // camera looks at
                0, 1, 0);   // up vector
    setJavaScriptMatrix4(_mWorld, _m);

    _m.setToIdentity();
    _m.perspective(
                60,
                this->width().value()/this->height().value(),
                1e-3,
                100);
    setJavaScriptMatrix4(_mProj, _m);

    _initializeUserSideMouseControl();

#else
    _mModel.setToIdentity();
    _mModel.scale(0.5, 0.5, 0.5);

    _mWorld.setToIdentity();
    _mWorld.lookAt(
                0, 0, 2,    // camera default position
                0, 0, 0,    // camera looks at
                0, 1, 0);   // up vector

    _mProj.setToIdentity();
    _mProj.perspective(
                60,
                this->width().value()/this->height().value(),
                1e-3,
                100);
#endif //USER_SIDE_CONTROL
}

#ifdef USER_SIDE_CONTROL
void VolumeRender::_drawBox(Program &program, JavaScriptMatrix4x4 &sceneMatrix)
#else
void VolumeRender::_drawBox(Program &program, WMatrix4x4 &sceneMatrix)
#endif //USER_SIDE_CONTROL
{
    AttribLocation _vrtPos = getAttribLocation(program, "aVrtPos");
    enableVertexAttribArray(_vrtPos);
    UniformLocation _uniformSceneMatrix =
            getUniformLocation(program, "uSceneMatrix");
    uniformMatrix4(_uniformSceneMatrix, sceneMatrix);

    bindBuffer(ARRAY_BUFFER, _boxVerticesBuffer);
    vertexAttribPointer(_vrtPos, 3, FLOAT, false, 0, 0);

    drawArrays(TRIANGLES, 0, 36);
}

void VolumeRender::paintGL()
{
#ifdef USER_SIDE_CONTROL
    _buildSceneMatrix();
#else
    using namespace boost::numeric::ublas;
    _mScene.impl() = prod(_mProj.impl(), _mWorld.impl());
    _mScene.impl() = prod(_mScene.impl(), _mControl.impl());
    _mScene.impl() = prod(_mScene.impl(), _mModel.impl());
#endif //USER_SIDE_CONTROL

    // Render First
    // Draw into fragment buffer the box world-space position as RGB colors

    cullFace(FRONT);
    bindFramebuffer(FRAMEBUFFER, _framebuffer);
    useProgram(_shaderProgramFirst);

    clearColor(0.0, 0.0, 0.0, 1);
    clear(COLOR_BUFFER_BIT | DEPTH_BUFFER_BIT);

    _drawBox(_shaderProgramFirst, _mScene);

    // Render Second

    cullFace(BACK);
    bindFramebuffer(FRAMEBUFFER, "null");
    useProgram(_shaderProgramSecond);

    clearColor(0.0, 0.0, 0.0, 1);
    clear(COLOR_BUFFER_BIT | DEPTH_BUFFER_BIT);

    UniformLocation _uniforBackfaceTextureWidth =
            getUniformLocation(_shaderProgramSecond, "uBackfaceTextureWidth");
    uniform1f(_uniforBackfaceTextureWidth, this->width().value());

    UniformLocation _uniforBackfaceTextureHeight =
            getUniformLocation(_shaderProgramSecond, "uBackfaceTextureHeight");
    uniform1f(_uniforBackfaceTextureHeight, this->height().value());

    UniformLocation _uniformSize =
            getUniformLocation(_shaderProgramSecond, "uSize");
    uniform1f(_uniformSize, _RVESize);

    UniformLocation _backfaceTextureSampler =
            getUniformLocation(_shaderProgramSecond, "uBackfaceTextureSampler");
    activeTexture(TEXTURE0);
    bindTexture(TEXTURE_2D, _framebufferTexture);
    uniform1i(_backfaceTextureSampler, 0);

    UniformLocation _volumeTextureSampler =
            getUniformLocation(_shaderProgramSecond, "uVolumeTextureSampler");
    activeTexture(TEXTURE1);
    bindTexture(TEXTURE_2D, _RVEtexture);
    uniform1i(_volumeTextureSampler, 1);

    UniformLocation _uniformInnerBottomCutLevel =
            getUniformLocation(_shaderProgramSecond, "uInnerBottomCutLevel");
    uniform1f(_uniformInnerBottomCutLevel, _innerBottomCutLevel);

    UniformLocation _uniformInnerTopCutLevel =
            getUniformLocation(_shaderProgramSecond, "uInnerTopCutLevel");
    uniform1f(_uniformInnerTopCutLevel, _innerTopCutLevel);

    _drawBox(_shaderProgramSecond, _mScene);
}

void VolumeRender::resizeGL(int width, int height)
{
    viewport(0, 0, width, height);
}

void VolumeRender::updateGL()
{

}

VolumeRender::~VolumeRender()
{
}

#ifdef USER_SIDE_CONTROL
void VolumeRender::_buildSceneMatrix()
{
    std::string _js;

    _js += WT_CLASS ".glMatrix.mat4.multiply(" + _mProj.jsRef() + ",";
    _js += _mWorld.jsRef() + ",";
    _js += _mScene.jsRef() + ");";

    _js += WT_CLASS ".glMatrix.mat4.multiply(" + _mScene.jsRef() + ",";
    _js += _mControl.jsRef() + ",";
    _js += _mScene.jsRef() + ");";

    _js += WT_CLASS ".glMatrix.mat4.multiply(" + _mScene.jsRef() + ",";
    _js += _mModel.jsRef() + ",";
    _js += _mScene.jsRef() + ");";

    injectJS(_js);
    GLDEBUG;
}

std::string VolumeRender::_glObjJsRef()
{
    return "(function(){"
        "var r = " + jsRef() + ";"
        "var o = r ? jQuery.data(r,'obj') : null;"
        "return o ? o : {ctx: null};"
            "})()";
}

void VolumeRender::_initializeUserSideMouseControl()
{
    // matrices should be already binded, see initializeGL()

    setJavaScriptMember("_omc","null");
    _onMouseWentDownJSlot.setJavaScript(
                "function(a, c){"
                "_omc=" WT_CLASS ".pageCoordinates(c);"
                "}");
    _onMouseDraggedJSlot.setJavaScript(
                "function(a, c){"
                "var d=" WT_CLASS ".pageCoordinates(c);"
                "var _ref=" + _glObjJsRef() + ";"
                "var _m=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1];"
                WT_CLASS ".glMatrix.mat4.rotate(_m,(d.y-_omc.y)/180.0,[1,0,0],_m);"
                WT_CLASS ".glMatrix.mat4.rotate(_m,(d.x-_omc.x)/180.0,[0,1,0],_m);"
                WT_CLASS ".glMatrix.mat4.multiply(_m,"
                "_ref." + _mControl.jsRef() + ","
                "_ref." + _mControl.jsRef() + ");"
                "_omc=d;"
                "_ref.paintGL();"
                "}");
    _onMouseWheelJSlot.setJavaScript(
                "function(a, c){"
                "var _ref=" + _glObjJsRef() + ";"
                "var _f=1+0.05*" WT_CLASS ".wheelDelta(c);"
                "var _m=[_f,0,0,0,0,_f,0,0,0,0,_f,0,0,0,0,1];"
                WT_CLASS ".glMatrix.mat4.multiply("
                "_ref." + _mControl.jsRef() + ","
                "_m,"
                "_ref." + _mControl.jsRef() + ");"
                "_ref.paintGL();"
                "}");

    mouseWentDown().connect(_onMouseWentDownJSlot);
    mouseDragged().connect(_onMouseDraggedJSlot);
    mouseWheel().connect(_onMouseWheelJSlot);
}
#else
void VolumeRender::_onMouseWentDown(const WMouseEvent &event)
{
    if(event.button() == WMouseEvent::LeftButton)
    {
        _oldMouseCoors[0] = event.screen().x;
        _oldMouseCoors[1] = event.screen().y;
    }
}

void VolumeRender::_onMouseDragged(const WMouseEvent &event)
{
    if(event.button() == WMouseEvent::LeftButton)
    {
        WMatrix4x4 _m;
        _m.rotate(event.screen().y-_oldMouseCoors[1], 1.0, 0.0, 0.0);
        _m.rotate(event.screen().x-_oldMouseCoors[0], 0.0, 1.0, 0.0);
        using namespace boost::numeric::ublas;
        _mControl.impl() = prod(_m.impl(), _mControl.impl());
        _oldMouseCoors[0] = event.screen().x;
        _oldMouseCoors[1] = event.screen().y;
        this->repaintGL(PAINT_GL);
    }
}

void VolumeRender::_onMouseWheel(const WMouseEvent &event)
{
    /// \todo remove constant, make it soft
//    _mControl.scale(1 + event.wheelDelta()*0.05);
//    _innerTopCutLevel += event.wheelDelta()*0.05;
//    if(_innerTopCutLevel > 1.0f) _innerTopCutLevel = 1.0f;
//    if(_innerTopCutLevel < _innerBottomCutLevel) _innerTopCutLevel = _innerBottomCutLevel;
    _innerBottomCutLevel += event.wheelDelta()*0.05;
    if(_innerBottomCutLevel < 0.0f) _innerBottomCutLevel = 0.0f;
    if(_innerBottomCutLevel > _innerTopCutLevel) _innerBottomCutLevel = _innerTopCutLevel;
    this->repaintGL(PAINT_GL);
}

#endif //USER_SIDE_CONTROL
