#!/bin/bash
#1505080 CSE314 Assignment 1

echo 1
#creating a working directory and copying necessary files

rm -rf workDir
mkdir workDir

cp submissionsAll.zip -t workDir
#cp CSE_322.csv -t workDir


cd workDir

mkdir Output
cd Output
mkdir Extra

touch AbsenteeList.txt
touch Marks.txt
touch FilesList.txt
touch TempList.txt

cd .. #workDir
pwd
cd .. #New1
pwd

cat CSE_322.csv | cut -d',' -f 1 | cut -d'"' -f 2 | cut  -d$'\t' -f2  > workDir/Output/AbsenteeList.txt
cd workDir

#ls -l

echo 2
#unzipping
unzip submissionsAll.zip
rm submissionsAll.zip

echo 3


#echo Zaara Tasnim_3000810_assignsubmission_file_1405019.zip | rev | cut -d'_' -f 1 | rev | cut -d'.' -f 1
#ls | cut -d'_' -f5 | cut -d'.' -f1

ls | while read line
do 
	#echo $line > FilesList.txt
	zipName=$line;
	#echo 5
	rollnum=$(echo $line | cut -d'_' -f5 | cut -d'.' -f1) 

	
	if  grep -qs $rollnum Output/AbsenteeList.txt 
	then
		#statements
		echo Found $rollnum
		echo $zipName >> Output/FilesList.txt

		sed "/$rollnum/d" "Output/AbsenteeList.txt" > Output/TempList.txt
	
		cat Output/TempList.txt > Output/AbsenteeList.txt

		mkdir temp

	
		cp "$zipName" -t temp

		cd temp
		unzip "$zipName" 

		rm "$zipName"

		dirName=$(ls)

		if [ $dirName = $rollnum ]; then
			echo Correct submission by $rollnum
			cp "$dirName" -rt ../Output
			echo $rollnum 10 >> ../Output/Marks.txt
		elif [ $dirname =~ $rollnum ]; then
			echo Partially correct submission by $rollnum
			cp "$dirName" -rt ../Output
			echo $rollnum 5 >> ../Output/Marks.txt

		else
			echo Wrong submission by $rollnum
			cp "$dirName" -rt ../Output/Extra
			echo $rollnum 0 >> ../Output/Marks.txt

		fi

		cd ..

		rm -rf temp


	fi


done
