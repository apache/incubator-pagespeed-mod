(function(){var c=window,d="/",e="//",g="A",h="keypress";c.pagespeed=c.pagespeed||{};var i=c.pagespeed,j=function(a){this.a=a};j.prototype.b=function(a){a=a||c.event;if(!(a.type==h&&13!=a.keyCode))for(var b=a.target;null!=b;b=b.parentNode)if(b.tagName==g){this.e(b.href,a);break}};j.prototype.c=function(){var a=this;document.body.onclick=function(b){a.b(b)};document.body.onkeypress=function(b){a.b(b)}};
j.prototype.e=function(a,b){for(var f=0;f<this.a.length;f++)if(0==a.indexOf(this.a[f])){c.location=c.location.protocol+e+c.location.hostname+d+a.substr(this.a[f].length);b.preventDefault();break}};i.d=function(a){a=new j(a);i.clientDomainRewriter=a;a.c()};i.clientDomainRewriterInit=i.d;})();
