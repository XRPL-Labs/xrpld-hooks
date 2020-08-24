/* eslint-disable no-var, no-param-reassign */
/* these eslint rules are disabled because gulp does not support babel yet */
'use strict'

var gulp = require('gulp')
var webpack = require('webpack-stream')
var uglify = require('gulp-uglify')
var rename = require('gulp-rename')
var pkg = require('./package.json')
var name = 'ripple-transaction-parser-' + pkg.version

gulp.task('build', function() {
  return gulp.src('./src/index.js')
    .pipe(webpack({
      output: {
        library: 'rippleTransactionParser',
        filename: name + '.js'
      }
    }))
    .pipe(gulp.dest('./dist/web/'))
})

gulp.task('build-min', ['build'], function() {
  return gulp.src('./dist/web/' + name + '.js')
    .pipe(uglify())
    .pipe(rename(name + '.min.js'))
    .pipe(gulp.dest('./dist/web/'))
})


gulp.task('default', ['build-min'])
