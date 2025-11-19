// Frangi Filter - ФИНАЛЬНАЯ ИСПРАВЛЕННАЯ ВЕРСИЯ v4
// Исправлены: Gradients, нормализация, контраст

class FrangiFilterApp {
    constructor() {
        this.canvas = document.getElementById('canvas');
        if (!this.canvas) {
            console.error('Canvas не найден!');
            return;
        }
        
        this.setupThreeJS();
        this.setupShaders();
        this.loadDefaultImage();
        this.setupUI();
        this.fpsCounter = new FPSCounter();
        console.log('✓ FrangiFilterApp инициализирован');
        this.animate();
    }
    
    setupThreeJS() {
        this.scene = new THREE.Scene();
        this.camera = new THREE.OrthographicCamera(-1, 1, 1, -1, 0.1, 1000);
        
        try {
            this.renderer = new THREE.WebGLRenderer({ 
                canvas: this.canvas,
                preserveDrawingBuffer: true,
                antialias: false,
                alpha: false,
                powerPreference: 'high-performance'
            });
            console.log('✓ WebGLRenderer создан');
        } catch(e) {
            console.error('Ошибка WebGL:', e);
            alert('WebGL не поддерживается вашим браузером');
            return;
        }
        
        this.renderer.setSize(this.canvas.clientWidth, this.canvas.clientHeight);
        this.renderer.setClearColor(0x000000);
        
        const w = 512, h = 512;
        const rtOptions = {
            minFilter: THREE.LinearFilter,
            magFilter: THREE.LinearFilter,
            format: THREE.RGBAFormat,
            type: THREE.FloatType
        };
        
        this.renderTargets = {
            input_gray: new THREE.WebGLRenderTarget(w, h, rtOptions),
            blur_x: new THREE.WebGLRenderTarget(w, h, rtOptions),
            blur_y: new THREE.WebGLRenderTarget(w, h, rtOptions),
            gradients: new THREE.WebGLRenderTarget(w, h, rtOptions),  // НОВЫЙ: отдельный буфер для градиентов
            hessian: new THREE.WebGLRenderTarget(w, h, rtOptions),
            vesselness: new THREE.WebGLRenderTarget(w, h, rtOptions)
        };
        
        this.geometry = new THREE.PlaneGeometry(2, 2);
        
        this.params = {
            sigma: 1.5,
            alpha: 0.5,
            gamma: 15.0,
            visualMode: 0
        };
        
        window.addEventListener('resize', () => this.onWindowResize());
    }
    
    setupShaders() {
        const vShader = `
            varying vec2 vUv;
            void main() {
                vUv = uv;
                gl_Position = vec4(position, 1.0);
            }`;
        
        this.meshes = {};
        
        // GRAYSCALE CONVERSION
        this.meshes.grayscale = new THREE.Mesh(
            this.geometry,
            new THREE.ShaderMaterial({
                uniforms: {
                    uTexture: { value: null }
                },
                vertexShader: vShader,
                fragmentShader: `
                    varying vec2 vUv;
                    uniform sampler2D uTexture;
                    
                    void main() {
                        vec4 color = texture2D(uTexture, vUv);
                        float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
                        gl_FragColor = vec4(gray, gray, gray, 1.0);
                    }`
            })
        );
        
        // BLUR X
        this.meshes.blur_x = new THREE.Mesh(
            this.geometry,
            new THREE.ShaderMaterial({
                uniforms: {
                    uTexture: { value: null },
                    uSigma: { value: 1.5 }
                },
                vertexShader: vShader,
                fragmentShader: `
                    varying vec2 vUv;
                    uniform sampler2D uTexture;
                    uniform float uSigma;
                    
                    void main() {
                        float h = 1.0 / 512.0;
                        vec4 sum = vec4(0.0);
                        float totalWeight = 0.0;
                        
                        for(int i = -25; i <= 25; i++) {
                            float offset = float(i) * h;
                            float weight = exp(-float(i*i) / (2.0 * uSigma * uSigma));
                            vec4 texel = texture2D(uTexture, vUv + vec2(offset, 0.0));
                            sum += texel * weight;
                            totalWeight += weight;
                        }
                        
                        gl_FragColor = sum / totalWeight;
                    }`
            })
        );
        
        // BLUR Y (только размытие, без градиентов!)
        this.meshes.blur_y = new THREE.Mesh(
            this.geometry,
            new THREE.ShaderMaterial({
                uniforms: {
                    uTexture: { value: null },
                    uSigma: { value: 1.5 }
                },
                vertexShader: vShader,
                fragmentShader: `
                    varying vec2 vUv;
                    uniform sampler2D uTexture;
                    uniform float uSigma;
                    
                    void main() {
                        float h = 1.0 / 512.0;
                        vec4 sum = vec4(0.0);
                        float totalWeight = 0.0;
                        
                        for(int i = -25; i <= 25; i++) {
                            float offset = float(i) * h;
                            float weight = exp(-float(i*i) / (2.0 * uSigma * uSigma));
                            sum += texture2D(uTexture, vUv + vec2(0.0, offset)) * weight;
                            totalWeight += weight;
                        }
                        
                        gl_FragColor = sum / totalWeight;
                    }`
            })
        );
        
        // GRADIENTS (отдельный шейдер)
        this.meshes.gradients = new THREE.Mesh(
            this.geometry,
            new THREE.ShaderMaterial({
                uniforms: {
                    uTexture: { value: null }
                },
                vertexShader: vShader,
                fragmentShader: `
                    varying vec2 vUv;
                    uniform sampler2D uTexture;
                    
                    void main() {
                        float h = 1.0 / 512.0;
                        
                        // Центральные разности для первых производных
                        float c = texture2D(uTexture, vUv).x;
                        float px = texture2D(uTexture, vUv + vec2(h, 0.0)).x;
                        float nx = texture2D(uTexture, vUv - vec2(h, 0.0)).x;
                        float py = texture2D(uTexture, vUv + vec2(0.0, h)).x;
                        float ny = texture2D(uTexture, vUv - vec2(0.0, h)).x;
                        
                        float fx = (px - nx) / (2.0 * h);
                        float fy = (py - ny) / (2.0 * h);
                        
                        // Выходной формат: fx в R, fy в G, blur в B
                        gl_FragColor = vec4(fx, fy, c, 1.0);
                    }`
            })
        );
        
        // HESSIAN (считает вторые производные из градиентов)
        this.meshes.hessian = new THREE.Mesh(
            this.geometry,
            new THREE.ShaderMaterial({
                uniforms: {
                    uTexture: { value: null }
                },
                vertexShader: vShader,
                fragmentShader: `
                    varying vec2 vUv;
                    uniform sampler2D uTexture;
                    
                    void main() {
                        float h = 1.0 / 512.0;
                        
                        // Читаем градиенты fx и fy
                        vec4 c = texture2D(uTexture, vUv);
                        vec4 px = texture2D(uTexture, vUv + vec2(h, 0.0));
                        vec4 nx = texture2D(uTexture, vUv - vec2(h, 0.0));
                        vec4 py = texture2D(uTexture, vUv + vec2(0.0, h));
                        vec4 ny = texture2D(uTexture, vUv - vec2(0.0, h));
                        
                        // fx находится в .x, fy находится в .y
                        float fxx = (px.x - nx.x) / (2.0 * h);  // d(fx)/dx
                        float fyy = (py.y - ny.y) / (2.0 * h);  // d(fy)/dy
                        
                        // fxy = d(fx)/dy = d(fy)/dx (симметричная матрица)
                        float fxy = (py.x - ny.x) / (2.0 * h);  // d(fx)/dy
                        
                        gl_FragColor = vec4(fxx, fxy, fyy, 1.0);
                    }`
            })
        );
        
        // VESSELNESS
        this.meshes.vesselness = new THREE.Mesh(
            this.geometry,
            new THREE.ShaderMaterial({
                uniforms: {
                    uTexture: { value: null },
                    uAlpha: { value: 0.5 },
                    uGamma: { value: 15.0 }
                },
                vertexShader: vShader,
                fragmentShader: `
                    varying vec2 vUv;
                    uniform sampler2D uTexture;
                    uniform float uAlpha;
                    uniform float uGamma;
                    
                    void main() {
                        vec4 h = texture2D(uTexture, vUv);
                        float fxx = h.x;
                        float fxy = h.y;
                        float fyy = h.z;
                        
                        // Собственные значения матрицы Гессиана [[fxx, fxy], [fxy, fyy]]
                        float tr = fxx + fyy;
                        float det = fxx * fyy - fxy * fxy;
                        
                        float disc = tr * tr - 4.0 * det;
                        if(disc < 0.0) disc = 0.0;
                        
                        float sqrtD = sqrt(disc);
                        float lambda1 = (tr + sqrtD) / 2.0;
                        float lambda2 = (tr - sqrtD) / 2.0;
                        
                        // Абсолютные значения
                        float al1 = abs(lambda1);
                        float al2 = abs(lambda2);
                        
                        // Гарантируем al1 >= al2
                        if(al1 < al2) {
                            float tmp = al1;
                            al1 = al2;
                            al2 = tmp;
                            tmp = lambda1;
                            lambda1 = lambda2;
                            lambda2 = tmp;
                        }
                        
                        float vesselness = 0.0;
                        
                        // Оба должны быть отрицательны (локальный минимум)
                        if(lambda1 < 0.0 && lambda2 < 0.0) {
                            float Rb = al2 / (al1 + 1e-6);
                            float S = sqrt(lambda1*lambda1 + lambda2*lambda2);
                            
                            float term1 = exp(-(Rb*Rb) / (2.0 * uAlpha * uAlpha));
                            float term2 = 1.0 - exp(-(S*S) / (2.0 * uGamma * uGamma));
                            
                            vesselness = term1 * term2;
                        }
                        
                        gl_FragColor = vec4(vec3(vesselness), 1.0);
                    }`
            })
        );
        
        // VISUALIZATION (улучшенная версия)
        this.meshes.visualize = new THREE.Mesh(
            this.geometry,
            new THREE.ShaderMaterial({
                uniforms: {
                    uTexture: { value: null },
                    uMode: { value: 0.0 }
                },
                vertexShader: vShader,
                fragmentShader: `
                    varying vec2 vUv;
                    uniform sampler2D uTexture;
                    uniform float uMode;
                    
                    void main() {
                        float v = texture2D(uTexture, vUv).x;
                        
                        // Нормализация без smoothstep
                        v = clamp(v, 0.0, 1.0);
                        
                        // Мягкое усиление контраста
                        v = v * v * (3.0 - 2.0 * v);  // smoothstep-like но мягче
                        
                        vec3 color;
                        
                        if(uMode < 0.5) {
                            // Vesselness (B/W)
                            color = vec3(v);
                        } else if(uMode < 1.5) {
                            // Heat map
                            if(v < 0.25) {
                                color = mix(vec3(0.0), vec3(0.0, 0.0, 1.0), v / 0.25);
                            } else if(v < 0.5) {
                                color = mix(vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 0.5), (v - 0.25) / 0.25);
                            } else if(v < 0.75) {
                                color = mix(vec3(0.0, 1.0, 0.5), vec3(1.0, 1.0, 0.0), (v - 0.5) / 0.25);
                            } else {
                                color = mix(vec3(1.0, 1.0, 0.0), vec3(1.0, 1.0, 1.0), (v - 0.75) / 0.25);
                            }
                        } else {
                            // Gradient
                            color = vec3(v, v * 0.8, v * 0.4);
                        }
                        
                        gl_FragColor = vec4(color, 1.0);
                    }`
            })
        );
        
        for(let key in this.meshes) {
            this.scene.add(this.meshes[key]);
        }
        
        console.log('✓ Шейдеры созданы');
    }
    
    loadDefaultImage() {
        const canvas = document.createElement('canvas');
        canvas.width = 512;
        canvas.height = 512;
        const ctx = canvas.getContext('2d');
        
        ctx.fillStyle = 'white';
        ctx.fillRect(0, 0, 512, 512);
        ctx.strokeStyle = 'black';
        ctx.lineWidth = 10;
        
        for(let i = 50; i < 500; i += 60) {
            ctx.beginPath();
            ctx.moveTo(i, 100);
            ctx.lineTo(i, 400);
            ctx.stroke();
            
            ctx.beginPath();
            ctx.moveTo(100, i);
            ctx.lineTo(400, i);
            ctx.stroke();
        }
        
        const texture = new THREE.CanvasTexture(canvas);
        texture.minFilter = THREE.LinearFilter;
        texture.magFilter = THREE.LinearFilter;
        this.inputTexture = texture;
        console.log('✓ Тестовое изображение загружено');
    }
    
    setupUI() {
        const sigma = document.getElementById('sigma');
        const alpha = document.getElementById('alpha');
        const gamma = document.getElementById('gamma');
        const visualMode = document.getElementById('visualMode');
        const loadBtn = document.getElementById('loadImageBtn');
        const resetBtn = document.getElementById('resetBtn');
        const downloadBtn = document.getElementById('downloadBtn');
        const imageInput = document.getElementById('imageInput');
        
        if(!sigma) {
            console.warn('UI элементы не найдены в HTML');
            return;
        }
        
        sigma.addEventListener('input', e => {
            this.params.sigma = parseFloat(e.target.value);
            document.getElementById('sigmaValue').textContent = this.params.sigma.toFixed(1);
        });
        
        alpha.addEventListener('input', e => {
            this.params.alpha = parseFloat(e.target.value);
            document.getElementById('alphaValue').textContent = this.params.alpha.toFixed(1);
        });
        
        gamma.addEventListener('input', e => {
            this.params.gamma = parseFloat(e.target.value);
            document.getElementById('gammaValue').textContent = this.params.gamma.toFixed(1);
        });
        
        visualMode.addEventListener('change', e => {
            this.params.visualMode = parseFloat(e.target.value);
            this.meshes.visualize.material.uniforms.uMode.value = this.params.visualMode;
        });
        
        loadBtn.addEventListener('click', () => {
            imageInput.click();
        });
        
        imageInput.addEventListener('change', e => {
            const file = e.target.files[0];
            if(file) {
                const reader = new FileReader();
                reader.onload = event => {
                    const img = new Image();
                    img.onload = () => {
                        const texture = new THREE.CanvasTexture(img);
                        texture.minFilter = THREE.LinearFilter;
                        texture.magFilter = THREE.LinearFilter;
                        this.inputTexture = texture;
                        console.log('✓ Изображение загружено:', img.width, 'x', img.height);
                    };
                    img.src = event.target.result;
                };
                reader.readAsDataURL(file);
            }
        });
        
        resetBtn.addEventListener('click', () => {
            sigma.value = 1.5;
            alpha.value = 0.5;
            gamma.value = 15.0;
            visualMode.value = 0;
            document.getElementById('sigmaValue').textContent = '1.5';
            document.getElementById('alphaValue').textContent = '0.5';
            document.getElementById('gammaValue').textContent = '15.0';
            this.params = { sigma: 1.5, alpha: 0.5, gamma: 15.0, visualMode: 0 };
            this.meshes.visualize.material.uniforms.uMode.value = 0;
            this.loadDefaultImage();
        });
        
        downloadBtn.addEventListener('click', () => {
            const link = document.createElement('a');
            link.href = this.canvas.toDataURL('image/png');
            link.download = `frangi-${Date.now()}.png`;
            link.click();
        });
        
        console.log('✓ UI настроен');
    }
    
    onWindowResize() {
        const w = this.canvas.clientWidth;
        const h = this.canvas.clientHeight;
        this.renderer.setSize(w, h);
        document.getElementById('resolution').textContent = `${w}x${h}`;
    }
    
    processFrame() {
        if(!this.inputTexture) return;
        
        // Pass 0: Grayscale
        this.meshes.grayscale.material.uniforms.uTexture.value = this.inputTexture;
        this.renderPass(this.meshes.grayscale, this.renderTargets.input_gray);
        
        this.meshes.blur_x.material.uniforms.uSigma.value = this.params.sigma;
        this.meshes.blur_y.material.uniforms.uSigma.value = this.params.sigma;
        this.meshes.vesselness.material.uniforms.uAlpha.value = this.params.alpha;
        this.meshes.vesselness.material.uniforms.uGamma.value = this.params.gamma;
        
        // Pass 1: Blur X
        this.meshes.blur_x.material.uniforms.uTexture.value = this.renderTargets.input_gray.texture;
        this.renderPass(this.meshes.blur_x, this.renderTargets.blur_x);
        
        // Pass 2: Blur Y
        this.meshes.blur_y.material.uniforms.uTexture.value = this.renderTargets.blur_x.texture;
        this.renderPass(this.meshes.blur_y, this.renderTargets.blur_y);
        
        // Pass 3: Gradients
        this.meshes.gradients.material.uniforms.uTexture.value = this.renderTargets.blur_y.texture;
        this.renderPass(this.meshes.gradients, this.renderTargets.gradients);
        
        // Pass 4: Hessian
        this.meshes.hessian.material.uniforms.uTexture.value = this.renderTargets.gradients.texture;
        this.renderPass(this.meshes.hessian, this.renderTargets.hessian);
        
        // Pass 5: Vesselness
        this.meshes.vesselness.material.uniforms.uTexture.value = this.renderTargets.hessian.texture;
        this.renderPass(this.meshes.vesselness, this.renderTargets.vesselness);
        
        // Pass 6: Display
        this.meshes.visualize.material.uniforms.uTexture.value = this.renderTargets.vesselness.texture;
        this.renderPass(this.meshes.visualize, null);
    }
    
    renderPass(mesh, target) {
        for(let key in this.meshes) {
            this.meshes[key].visible = (this.meshes[key] === mesh);
        }
        this.renderer.setRenderTarget(target);
        this.renderer.render(this.scene, this.camera);
    }
    
    animate = () => {
        requestAnimationFrame(this.animate);
        try {
            this.processFrame();
            this.fpsCounter.update();
            document.getElementById('fps').textContent = this.fpsCounter.fps;
        } catch(e) {
            console.error('Ошибка при рендеринге:', e);
        }
    }
}

class FPSCounter {
    constructor() {
        this.fps = 0;
        this.frames = 0;
        this.lastTime = performance.now();
    }
    
    update() {
        this.frames++;
        const time = performance.now();
        if(time >= this.lastTime + 1000) {
            this.fps = this.frames;
            this.frames = 0;
            this.lastTime = time;
        }
    }
}

window.addEventListener('DOMContentLoaded', () => {
    console.log('DOM загружен, инициализация...');
    new FrangiFilterApp();
});

console.log('Скрипт загружен');