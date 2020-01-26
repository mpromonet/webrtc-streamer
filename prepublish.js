// compute version
var getRepoInfo = require("git-repo-info");
var info = getRepoInfo();
var version="1.0.0-"+info.branch+"."+info.abbreviatedSha;
if (info.tag) {
  version=info.tag;
} else if (info.lastTag) {
  version=info.lastTag;
  if (info.commitsSinceLastTag) {
	  version+="-"+info.commitsSinceLastTag;
  }
}

// update version
var npm = require("npm");
npm.load({}, function (err) {
        npm.config.set("git-tag-version",false);
        npm.config.set("allow-same-version",true);
        npm.commands.version([version], (err) => {
                if (err) {
			console.log(err);
		}
        });
});
