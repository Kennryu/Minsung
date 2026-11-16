import * as THREE from 'three';

export function rr(w,h,r){const s=new THREE.Shape();const x=-w/2,y=-h/2;
  s.moveTo(x+r,y);s.lineTo(x+w-r,y);s.quadraticCurveTo(x+w,y,x+w,y+r);s.lineTo(x+w,y+h-r);s.quadraticCurveTo(x+w,y+h,x+w-r,y+h);
  s.lineTo(x+r,y+h);s.quadraticCurveTo(x,y+h,x,y+h-r);s.lineTo(x,y+r);s.quadraticCurveTo(x,y,x+r,y);return s;}
export function circ(r){const s=new THREE.Shape();s.absarc(0,0,r,0,Math.PI*2);return s;}
// soft extruded slab from z0 to z1
export function slab(shape,z0,z1,bev,mat,segs=6){
  const depth=Math.max(0.01,(z1-z0)-2*bev);
  const g=new THREE.ExtrudeGeometry(shape,{depth,bevelEnabled:true,bevelThickness:bev,bevelSize:bev,bevelSegments:segs,curveSegments:48});
  const m=new THREE.Mesh(g,mat);m.position.z=z0+bev;m.castShadow=true;m.receiveShadow=true;return m;}
export function canvasTex(w,h,draw){const c=document.createElement('canvas');c.width=w;c.height=h;const x=c.getContext('2d');draw(x,w,h);
  const t=new THREE.CanvasTexture(c);t.colorSpace=THREE.SRGBColorSpace;t.anisotropy=8;return t;}
function padlock(x,cx,cy,s,open,col){x.strokeStyle=col;x.fillStyle=col;x.lineWidth=7*s;x.lineCap='round';x.beginPath();
  if(open){x.moveTo(cx-18*s,cy);x.lineTo(cx-18*s,cy-22*s);x.arc(cx,cy-22*s,18*s,Math.PI,Math.PI*1.85);}
  else{x.moveTo(cx-18*s,cy);x.lineTo(cx-18*s,cy-22*s);x.arc(cx,cy-22*s,18*s,Math.PI,0);x.lineTo(cx+18*s,cy);}
  x.stroke();x.beginPath();x.roundRect(cx-26*s,cy-4*s,52*s,40*s,8*s);x.fill();}
function micIcon(x,cx,cy,s,col){x.fillStyle=col;x.strokeStyle=col;x.lineWidth=7*s;x.lineCap='round';
  x.beginPath();x.roundRect(cx-14*s,cy-40*s,28*s,50*s,14*s);x.fill();
  x.beginPath();x.arc(cx,cy-8*s,26*s,0.1,Math.PI-0.1);x.stroke();x.beginPath();x.moveTo(cx,cy+18*s);x.lineTo(cx,cy+34*s);x.stroke();}

export function makeRemote(){
  const g=new THREE.Group();
  const W=54,L=125,T=22;
  const bodyMat=new THREE.MeshPhysicalMaterial({color:0x2b2e35,roughness:0.42,metalness:0.0,clearcoat:0.35,clearcoatRoughness:0.35});
  const coreMat=new THREE.MeshStandardMaterial({color:0x0b0b0d,roughness:0.8});
  const keyMat=new THREE.MeshPhysicalMaterial({color:0x3b3e46,roughness:0.62,sheen:0.4,sheenColor:0x888888,sheenRoughness:0.6});
  const gapMat=new THREE.MeshStandardMaterial({color:0x060607,roughness:0.95});
  const B=4.2;
  g.add(slab(rr(W-2*B,L-2*B,15-B),0,T,B,bodyMat,12));
  const seamS=rr(W+0.3,L+0.3,15.15);seamS.holes.push(rr(W-3,L-3,13.5));
  const seam=new THREE.Mesh(new THREE.ExtrudeGeometry(seamS,{depth:0.45,bevelEnabled:false,curveSegments:64}),coreMat);seam.position.z=10.8;g.add(seam);
  const top=T;const Y=v=>L/2-v;
  const flat=(shape,z,mat)=>{const m=new THREE.Mesh(new THREE.ShapeGeometry(shape,64),mat);m.position.z=z;m.receiveShadow=true;return m;};
  // LED
  const led=new THREE.Mesh(new THREE.CylinderGeometry(1.7,1.7,0.6,32),new THREE.MeshStandardMaterial({color:0x8ce99a,emissive:0x40ff70,emissiveIntensity:4}));
  led.rotation.x=Math.PI/2;led.position.set(0,Y(11),top+0.1);g.add(led);
  g.add(Object.assign(flat(circ(2.6),top+0.02,gapMat),{}));g.children.at(-1).position.y=Y(11);
  // mic holes
  for(let r=0;r<2;r++)for(let c=0;c<7-r;c++){const h=flat(circ(0.65),top+0.03,gapMat);h.position.set(-9+c*3+r*1.5,Y(20)-r*2.8,top+0.03);g.add(h);}
  // voice key
  const vy=Y(48);
  const gap1=flat(circ(14.2),top+0.03,gapMat);gap1.position.y=vy;g.add(gap1);
  const ring=new THREE.Mesh(new THREE.TorusGeometry(13.35,0.45,16,96),new THREE.MeshStandardMaterial({color:0x74c0fc,emissive:0x2f9bff,emissiveIntensity:3.2}));
  ring.position.set(0,vy,top+0.15);g.add(ring);
  const vk=slab(circ(11.2),top-0.5,top+2.6,1.2,keyMat,8);vk.position.y=vy;g.add(vk);
  const micT=canvasTex(256,256,(x,w,h)=>{micIcon(x,128,136,1.5,'#8fd0ff');});
  const mp=new THREE.Mesh(new THREE.CircleGeometry(9,48),new THREE.MeshBasicMaterial({map:micT,transparent:true,toneMapped:false,color:0xdddddd}));
  mp.position.set(0,vy,top+2.62);g.add(mp);
  // rect keys
  const rkey=(v,open,label,col)=>{const y=Y(v);
    const gp=flat(rr(36.5,19.5,9.7),top+0.03,gapMat);gp.position.y=y;g.add(gp);
    const k=slab(rr(34-2.4,17-2.4,7.3),top-0.5,top+2.4,1.2,keyMat,8);k.position.y=y;g.add(k);
    const t=canvasTex(512,256,(x,w,h)=>{padlock(x,150,140,1.25,open,col);x.fillStyle='#e9ecef';x.font='bold 92px "WenQuanYi Zen Hei"';x.textBaseline='middle';x.fillText(label,215,138);});
    const p=new THREE.Mesh(new THREE.PlaneGeometry(30,15),new THREE.MeshBasicMaterial({map:t,transparent:true,toneMapped:false,color:0xcccccc}));
    p.position.set(0,y,top+2.42);g.add(p);};
  rkey(78,true,'열림','#5ee87a');rkey(100,false,'잠금','#ff6b6b');
  // engraved text
  const tt=canvasTex(1024,128,(x,w,h)=>{x.fillStyle='#6b707a';x.font='54px "WenQuanYi Zen Hei"';x.textAlign='center';x.textBaseline='middle';x.letterSpacing='18px';x.fillText('SMART DOOR KEY',w/2,h/2);});
  const tp=new THREE.Mesh(new THREE.PlaneGeometry(32,4),new THREE.MeshBasicMaterial({map:tt,transparent:true,toneMapped:false,color:0x999999}));
  tp.position.set(0,Y(116.5),top+0.04);g.add(tp);
  // USB-C on bottom end
  const usb=new THREE.Mesh(new THREE.ShapeGeometry(rr(9,3.3,1.6),32),gapMat);usb.rotation.x=Math.PI/2;usb.position.set(0,-L/2-0.05,11);g.add(usb);
  const usbIn=new THREE.Mesh(new THREE.ShapeGeometry(rr(5.6,0.8,0.4),16),new THREE.MeshStandardMaterial({color:0xb0b5bb,metalness:1,roughness:0.3}));usbIn.rotation.x=Math.PI/2;usbIn.position.set(0,-L/2-0.1,11);g.add(usbIn);
  // side switch
  const sw=new THREE.Mesh(new THREE.BoxGeometry(0.8,8,3.2),gapMat);sw.position.set(W/2+0.05,Y(32),11);g.add(sw);
  const swk=new THREE.Mesh(new THREE.BoxGeometry(1.4,3.4,2),new THREE.MeshStandardMaterial({color:0x9aa0a6,metalness:0.6,roughness:0.4}));swk.position.set(W/2+0.4,Y(30),11);g.add(swk);
  // lanyard hole
  const lh=new THREE.Mesh(new THREE.CylinderGeometry(1.8,1.8,1,24),gapMat);lh.position.set(W/2-9,-L/2-0.2,4.5);g.add(lh);
  g.traverse(o=>{if(o.isMesh){o.castShadow=true;}});
  return g;
}
