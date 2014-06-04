(function(){var g=navigator,h=String,k="shift",l="replace",m="userAgent",n="stack",p="console",q="fileName",r="push",s="indexOf",t="length",u="prototype",v="target",w="call",x="navigator",y="",aa="\n\nBrowser stack:\n",ba='\nUrl: <a href="view-source:',ca='"',da='" target="_new">',ea="%s",fa="&",ga="&amp;",ha="&gt;",ia="&lt;",ja="&quot;",ka="(",la=")",ma=")\n",z=", ",na="-> ",oa=".",pa="...",qa=": ",ra="<",sa="</a>\nLine: ",ta=">",ua="Assertion failed",va="DOMFocusIn",wa="DOMFocusOut",xa="Exception trying to expose exception! You win, we lose. ",
ya="Message: ",A="Not available",za="Not busting click on label elem at (",Aa="Root logger has no level set.",Ba="TEXTAREA",Ca="Unknown error",Da="[...circular reference...]",Ea="[...long stack...]",Fa="[Anonymous]",Ga="[end]",Ha="[end]\n\nJS stack traversal:\n",Ia="[exception trying to get caller]\n",Ja="[fn]",Ka="boolean",La="busting click at ",Ma="false",Na="function",Oa="href",Pa="location",Qa="log:",Ra="null",Sa="number",Ta="object",B="string",Ua="true",Va="window",C,D=this,E=Date.now||function(){return+new Date},
Wa=function(a,b){function c(){}c.prototype=b[u];a.r=b[u];a.prototype=new c};var F=function(a){Error.captureStackTrace?Error.captureStackTrace(this,F):this.stack=Error()[n]||y;a&&(this.message=h(a))};Wa(F,Error);F[u].name="CustomError";var Xa=function(a,b){for(var c=a.split(ea),f=y,d=Array[u].slice[w](arguments,1);d[t]&&1<c[t];)f+=c[k]()+d[k]();return f+c.join(ea)},G=function(a,b){if(b)return a[l](Ya,ga)[l](Za,ia)[l]($a,ha)[l](ab,ja);if(!bb.test(a))return a;-1!=a[s](fa)&&(a=a[l](Ya,ga));-1!=a[s](ra)&&(a=a[l](Za,ia));-1!=a[s](ta)&&(a=a[l]($a,ha));-1!=a[s](ca)&&(a=a[l](ab,ja));return a},Ya=/&/g,Za=/</g,$a=/>/g,ab=/\"/g,bb=/[&<>\"]/;var H=function(a,b){b.unshift(a);F[w](this,Xa.apply(null,b));b[k]()};Wa(H,F);H[u].name="AssertionError";var cb=function(a,b,c){if(!a){var f=Array[u].slice[w](arguments,2),d=ua;if(b)var d=d+(qa+b),e=f;throw new H(y+d,e||[]);}return a},db=function(a,b){throw new H("Failure"+(a?qa+a:y),Array[u].slice[w](arguments,1));};var eb=Array[u],fb=eb[s]?function(a,b,c){cb(null!=a[t]);return eb[s][w](a,b,c)}:function(a,b,c){c=null==c?0:0>c?Math.max(0,a[t]+c):c;if(typeof a==B)return typeof b==B&&1==b[t]?a[s](b,c):-1;for(;c<a[t];c++)if(c in a&&a[c]===b)return c;return-1};var I,J,K,L,gb=function(){return D[x]?D[x][m]:null};L=K=J=I=!1;var M;if(M=gb()){var hb=D[x];I=0==M[s]("Opera");J=!I&&-1!=M[s]("MSIE");K=!I&&-1!=M[s]("WebKit");L=!I&&!K&&"Gecko"==hb.product}var ib=J,jb=L,kb=K;var O;if(I&&D.opera){var lb=D.opera.version;typeof lb==Na&&lb()}else jb?O=/rv\:([^\);]+)(\)|;)/:ib?O=/MSIE\s+([^\);]+)(\)|;)/:kb&&(O=/WebKit\/(\S+)/),O&&O.exec(gb());var nb=function(a,b){try{var c;var f;t:{for(var d=[Va,Pa,Oa],e=D,N;N=d[k]();)if(null!=e[N])e=e[N];else{f=null;break t}f=e}if(typeof a==B)c={message:a,name:Ca,lineNumber:A,fileName:f,stack:A};else{var V,W,d=!1;try{V=a.lineNumber||a.q||A}catch(Hb){V=A,d=!0}try{W=a[q]||a.filename||a.sourceURL||D.$googDebugFname||f}catch(Ib){W=A,d=!0}c=!d&&a.lineNumber&&a[q]&&a[n]?a:{message:a.message,name:a.name,lineNumber:V,fileName:W,stack:a[n]||A}}return ya+G(c.message)+ba+c[q]+da+c[q]+sa+c.lineNumber+aa+G(c[n]+na)+
Ha+G(mb(b)+na)}catch(wb){return xa+wb}},mb=function(a){return ob(a||arguments.callee.caller,[])},ob=function(a,b){var c=[];if(0<=fb(b,a))c[r](Da);else if(a&&50>b[t]){c[r](pb(a)+ka);for(var f=a.arguments,d=0;d<f[t];d++){0<d&&c[r](z);var e;e=f[d];switch(typeof e){case Ta:e=e?Ta:Ra;break;case B:break;case Sa:e=h(e);break;case Ka:e=e?Ua:Ma;break;case Na:e=(e=pb(e))?e:Ja;break;default:e=typeof e}40<e[t]&&(e=e.substr(0,40)+pa);c[r](e)}b[r](a);c[r](ma);try{c[r](ob(a.caller,b))}catch(N){c[r](Ia)}}else a?
c[r](Ea):c[r](Ga);return c.join(y)},pb=function(a){if(P[a])return P[a];a=h(a);if(!P[a]){var b=/function ([^\(]+)/.exec(a);P[a]=b?b[1]:Fa}return P[a]},P={};var Q=function(a,b,c,f,d){this.reset(a,b,c,f,d)};Q[u].b=null;Q[u].a=null;var qb=0;C=Q[u];C.reset=function(a,b,c,f,d){typeof d==Sa||qb++;f||E();this.c=a;this.d=b;delete this.b;delete this.a};C.n=function(a){this.b=a};C.o=function(a){this.a=a};C.f=function(a){this.c=a};C.getMessage=function(){return this.d};var R=function(a){this.k=a};R[u].a=null;R[u].c=null;R[u].b=null;R[u].d=null;var S=function(a,b){this.name=a;this.value=b};S[u].toString=function(){return this.name};var rb=new S("WARNING",900),sb=new S("CONFIG",700);C=R[u];C.getParent=function(){return this.a};C.getChildren=function(){this.b||(this.b={});return this.b};C.f=function(a){this.c=a};C.e=function(){if(this.c)return this.c;if(this.a)return this.a.e();db(Aa);return null};C.m=function(a){return a.value>=this.e().value};
C.log=function(a,b,c){this.m(a)&&this.j(this.l(a,b,c))};C.l=function(a,b,c){var f=new Q(a,h(b),this.k);c&&(f.n(c),f.o(nb(c,arguments.callee.caller)));return f};C.g=function(a,b){this.log(rb,a,b)};C.j=function(a){var b=Qa+a.getMessage();D[p]&&(D[p].timeStamp?D[p].timeStamp(b):D[p].markTimeline&&D[p].markTimeline(b));D.msWriteProfilerMark&&D.msWriteProfilerMark(b);for(b=this;b;)b.p(a),b=b.getParent()};C.p=function(a){if(this.d)for(var b=0,c;c=this.d[b];b++)c(a)};C.i=function(a){this.a=a};
C.h=function(a,b){this.getChildren()[a]=b};var T={},U=null,tb=function(a){U||(U=new R(y),T[y]=U,U.f(sb));var b;if(!(b=T[a])){b=new R(a);var c=a.lastIndexOf(oa),f=a.substr(c+1),c=tb(a.substr(0,c));c.h(f,b);b.i(c);T[a]=b}return b};var X=function(a,b){this.x=void 0!==a?a:0;this.y=void 0!==b?b:0};X[u].toString=function(){return ka+this.x+z+this.y+la};var Y=function(a,b,c,f,d){var e=!!f;a.addEventListener(b,c,e);d&&(Y(a,va,function(d){d[v]&&d[v].tagName==Ba&&a.removeEventListener(b,c,e)}),Y(a,wa,function(d){d[v]&&d[v].tagName==Ba&&a.addEventListener(b,c,e)}))};var ub=/Mac OS X.+Silk\//;var vb=/iPhone|iPod|iPad/.test(g[m])||-1!=g[m][s]("Android")||ub.test(g[m]),xb=window[x].msPointerEnabled,yb=vb?"touchstart":xb?"MSPointerDown":"mousedown",zb=vb?"touchend":xb?"MSPointerUp":"mouseup",Ab=function(a){return function(b){b.touches=[];b.targetTouches=[];b.changedTouches=[];b.type!=zb&&(b.touches[0]=b,b.targetTouches[0]=b);b.changedTouches[0]=b;a(b)}};var Z,Bb,Cb,Db=tb("wireless.events.clickbuster"),Eb=function(a){if(!(2500<E()-Bb)){var b=new X(a.clientX,a.clientY);if(1>b.x&&1>b.y)Db.g(za+b.x+z+b.y+la);else{for(var c=0;c<Z[t];c+=2)if(25>Math.abs(b.x-Z[c])&&25>Math.abs(b.y-Z[c+1])){Z.splice(c,c+2);return}Db.g(La+b.x+z+b.y);a.stopPropagation();a.preventDefault();(a=Cb)&&a()}}},Fb=function(a){var b=new X((a.touches||[a])[0].clientX,(a.touches||[a])[0].clientY);Z[r](b.x,b.y);window.setTimeout(function(){for(var a=b.x,f=b.y,d=0;d<Z[t];d+=2)if(Z[d]==
a&&Z[d+1]==f){Z.splice(d,d+2);break}Cb=void 0},2500)};Cb=void 0;if(!Z){document.addEventListener("click",Eb,!0);var Gb=Fb;vb||xb||(Gb=Ab(Gb));Y(document,yb,Gb,!0,!0);Z=[]}Bb=E();for(var $=0;$<Z[t];$+=2)if(25>Math.abs(0-Z[$])&&25>Math.abs(0-Z[$+1])){Z.splice($,$+2);break};})();