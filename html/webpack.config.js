const path = require('path');

module.exports = {
  entry: './index.js',
  module: {
    rules: [
      {
        test: /\.(js|jsx)$/,
        exclude: /node_modules(?!\/webpack-proxy-client)/,
        use: {
          loader: 'babel-loader',
        }
      }
    ]
  },
  resolve: {
    extensions: ['*', '.js', '.jsx']
  },
  mode: 'development',
  output: {
    filename: 'index.js',
    path: path.resolve(__dirname, 'build')
  }
};
