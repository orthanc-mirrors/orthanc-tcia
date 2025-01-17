/**
 * TCIA plugin for Orthanc
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


// https://stackoverflow.com/a/13818704/881731
function globStringToRegex(str) {
  function preg_quote (str, delimiter) {
    // http://kevin.vanzonneveld.net
    // +   original by: booeyOH
    // +   improved by: Ates Goral (http://magnetiq.com)
    // +   improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
    // +   bugfixed by: Onno Marsman
    // +   improved by: Brett Zamir (http://brett-zamir.me)
    // *     example 1: preg_quote("$40");
    // *     returns 1: '\$40'
    // *     example 2: preg_quote("*RRRING* Hello?");
    // *     returns 2: '\*RRRING\* Hello\?'
    // *     example 3: preg_quote("\\.+*?[^]$(){}=!<>|:");
    // *     returns 3: '\\\.\+\*\?\[\^\]\$\(\)\{\}\=\!\<\>\|\:'
    return (str + '').replace(new RegExp('[.\\\\+*?\\[\\^\\]$(){}=!<>|:\\' + (delimiter || '') + '-]', 'g'), '\\$&');
  }

  return new RegExp(preg_quote(str, '').replace(/\\\*/g, '.*').replace(/\\\?/g, '.'), 'gi');
}



var app = new Vue({
  el: '#app',
  data: function() {
    return  {
      // Used by import
      jobId : '',
      importedPatients : [],

      // Used by explorer
      filter : '',
      collections : [],
      activeCollection : '',
      activePatientId : '',
      patients : [],
      studies : [],
      series : {},
      openedStudies : {},
      selectedSeries : {}
    }
  },

  computed: {
    allSelectedSeries: function() {
      var count = 0;
      for (var seriesInstanceUid in this.selectedSeries) {
        if (this.selectedSeries[seriesInstanceUid] === true) {
          count ++;
        }
      }
      return count;
    }
  },
  
  methods: {
    openJob: function() {
      if (this.jobId != '') {
        window.open('../../app/explorer.html#job?uuid=' + this.jobId, '_blank');
      }
    },

    refreshSeriesCount: function() {
      var that = this;

      // The use of "let" instead of "var" is mandatory so that the
      // scope of "i" matches that of the "for" loop
      for (let i = 0; i < that.importedPatients.length; i++) {
        axios.post('../../tools/find', {
          'Expand' : true,
          'Level' : 'Series',
          'Query' : {
            'PatientID' : that.importedPatients[i].PatientID
          }
        })
          .then(function(series) {
            var availableSeries = new Set();
            for (var j = 0; j < series.data.length; j++) {
              availableSeries.add(series.data[j].MainDicomTags.SeriesInstanceUID);
            }
            
            var count = 0;
            for (var j = 0; j < that.importedPatients[i].SeriesInstanceUID.length; j++) {
              if (availableSeries.has(that.importedPatients[i].SeriesInstanceUID[j])) {
                count ++;
              }
            }

            that.importedPatients[i].CompletedSeries = count;
          });
      }
    },

    getJobPatients: function() {
      this.importedPatients = [];

      if (this.jobId == '') {
        return;
      }
      
      var that = this;
      
      axios.get('../../jobs/' + this.jobId)
        .then(function(job) {
          var series = job.data.Content.Series;
          var index = {};
          
          for (var i = 0; i < series.length; i++) {
            var id = (series[i].Collection + '|' + series[i].PatientID);
            
            if (id in index) {
              var patient = index[id];
              patient.SeriesInstanceUID.push(series[i].SeriesInstanceUID);
              patient.InstancesCount += series[i].InstancesCount;
              patient.Size += parseInt(series[i].Size, 10);
            }
            else {
              index[id] = {
                'Collection' : series[i].Collection,
                'PatientID' : series[i].PatientID,
                'OrthancID' : series[i].OrthancID,
                'CompletedSeries' : 0,
                'SeriesInstanceUID' : [ series[i].SeriesInstanceUID ],
                'InstancesCount' : series[i].InstancesCount,
                'Size' : parseInt(series[i].Size, 10)
              }
            }
          }

          that.importedPatients = Object.values(index);
          
          that.importedPatients.sort(function(a, b) {
            if (a.Collection < b.Collection) {
              return -1;
            }
            else if (a.Collection > b.Collection) {
              return 1;
            }
            else {
              return a.PatientID < b.PatientID ? -1 : 1;
            }
          });

          that.refreshSeriesCount();
        });
    },
    
    importCart: function() {
      var that = this;
      
      var blob = this.$refs.cart.files[0];
      if (blob === undefined) {
        alert('No cart was provided');
      }
      else {
        var reader = new FileReader();
        reader.readAsText(blob);
        reader.onerror = function (error) {
          alert('Cannot read the cart');
        };
        reader.onload = function () {
          that.jobId = '';
          that.importedPatients = [];
          
          var base64 = btoa(reader.result);
          
          axios.post('../import', {
            'Type' : 'NbiaClientSpreadsheet',
            'Content' : base64,
            'Asynchronous' : true
          })
            .then(function(response) {
              that.jobId = response.data.ID;
              that.getJobPatients();
              window.location.href = '#import-status';
            })
            .catch(function(error) {
              alert('Cannot process the cart, check that this is a valid NBIA spreadsheet file in CSV format');
            });
        };
      }
    },

    concatenateTcia : function(arr, field) {
      var s = '';

      arr.sort(function(a, b) {
        if (a[field] < b[field]) {
          return -1;
        }
        else if (a[field] > b[field]) {
          return 1;
        }
        else {
          return 0;
        }
      });

      for (var i = 0; i < arr.length; i++) {
        if (field in arr[i]) {
          if (s != '') {
            s += ', ';
          }

          s += arr[i][field];
        }
      }

      return s;
    },

    listCollections : function() {
      this.activeCollection = '';
      this.filter = '';
      window.location.href = '#explore-tcia';
    },

    listPatients : function() {
      this.activePatientId = '';
      this.filter = '';
      window.location.href = '#explore-tcia';
    },

    openCollection : function(collection) {
      var that = this;
      
      axios.get('../proxy/getPatient', {
        params : {
          Collection : collection
        }
      })
        .then(function(patients) {
          that.activeCollection = collection;
          that.patients = patients.data;
          that.filter = '';
          window.location.href = '#explore-tcia';
        });
    },

    openPatient : function(patientId) {
      var that = this;
      
      axios.get('../proxy/getPatientStudy', {
        params : {
          Collection : this.activeCollection,
          PatientID : patientId
        }
      })
        .then(function(studies) {
          that.filter = '';
          that.activePatientId = patientId;
          that.studies = studies.data;
          that.openedStudies = {};
          that.series = {};
          that.selectedSeries = {};
          window.location.href = '#explore-tcia';
          
          axios.get('../proxy/getSeries', {
            params : {
              Collection : this.activeCollection,
              PatientID : patientId
            }
          })
            .then(function(series) {
              that.series = {};
              for (var i = 0; i < series.data.length; i++) {
                var studyInstanceUid = series.data[i].StudyInstanceUID;
                if (!(studyInstanceUid in that.series)) {
                  that.series[studyInstanceUid] = [];
                }
                that.series[studyInstanceUid].push(series.data[i]);
              }
            });
        });
    },

    openStudy: function(studyInstanceUid) {
      this.$set(this.openedStudies, studyInstanceUid, true);
    },

    closeStudy: function(studyInstanceUid) {
      this.$set(this.openedStudies, studyInstanceUid, false);
    },

    countSelectedSeries: function(studyInstanceUid) {
      var count = 0;
      if (studyInstanceUid in this.series) {
        for (var i = 0; i < this.series[studyInstanceUid].length; i++) {
          if (this.selectedSeries[this.series[studyInstanceUid][i].SeriesInstanceUID] === true) {
            count ++;
          }
        }
        return count;
      } else {
        return 0;
      }
    },
    
    setAllSeries: function(studyInstanceUid, isSelected) {
      if (studyInstanceUid in this.series) {
        for (var i = 0; i < this.series[studyInstanceUid].length; i++) {
          this.$set(this.selectedSeries, this.series[studyInstanceUid][i].SeriesInstanceUID, isSelected);
        }
      }
    },

    importSelectedSeries: function() {
      var content = [];

      for (var studyInstanceUid in this.series) {
        var series = this.series[studyInstanceUid];
        for (var i = 0; i < series.length; i++) {
          if (this.selectedSeries[series[i].SeriesInstanceUID] === true) {
            var size = 0;  // TODO
            
            content.push({
              'Collection' : this.activeCollection,
              'PatientID' : this.activePatientId,
              'SeriesInstanceUID' : series[i].SeriesInstanceUID,
              'InstancesCount' : series[i].ImageCount,
              'Size' : size.toString()
            });
          }
        }
      }

      var that = this;
      this.jobId = '';
      this.importedPatients = [];

      axios.post('../import', {
        'Type' : 'Series',
        'Content' : content,
        'Asynchronous' : true
      })
        .then(function(response) {
          that.jobId = response.data.ID;
          that.getJobPatients();
          window.location.href = '#import-status';
        })
        .catch(function(error) {
          alert('Cannot import the selected series');
        });
    },

    clearCache: function() {
      axios.post('../clear-cache', {})
        .catch(function(error) {
          alert('Cannot clear the cache');
        });
    }
  },

  mounted: function() {
    var that = this;
    
    axios.get('../proxy/getCollectionValues')
      .then(function(collections) {
        // Only use 1 axios query at a time, in order to avoid
        // overwhelming the browser
        
        function getModalityValues(i) {
          if (i < that.collections.length) {
            var collection = that.collections[i];
            
            axios.get('../proxy/getModalityValues', {
              params : {
                Collection : collection.Name
              }
            })
              .then(function(modalities) {
                var content = that.collections[i];
                content.Modalities = that.concatenateTcia(modalities.data, 'Modality');
                that.$set(that.collections, i, content);
                getModalityValues(i + 1);
              });
          }
        }

        function getBodyPartValues(i) {
          if (i < that.collections.length) {
            var collection = that.collections[i];
            
            axios.get('../proxy/getBodyPartValues', {
              params : {
                Collection : collection.Name
              }
            })
              .then(function(parts) {
                var content = that.collections[i];
                content.BodyParts = that.concatenateTcia(parts.data, 'BodyPartExamined');
                that.$set(that.collections, i, content);
                getBodyPartValues(i + 1);
              });
          }
        }

        for (let i = 0; i < collections.data.length; i++) {
          that.$set(that.collections, i, {
            Name : collections.data[i].Collection,
            Modalities : '...',
            BodyParts : '...'
          });
        }

        getModalityValues(0);
        getBodyPartValues(0);
      });
  }
})
