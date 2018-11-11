#!/bin/bash
#1505080 CSE314 Assignment 1

echo 1
#creating a working directory and copying necessary files

rm -rf workDir
mkdir workDir

cp submissionsAll.zip -t workDir
#cp CSE_322.csv -t workDir


cd workDir

mkdir ZipsWithoutRoll
mkdir Output
cd Output
mkdir Extra

touch AbsenteeList.txt
touch Marks.txt
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
	zipName=$line;
	#echo 5
	rollnum=$(echo $line | cut -d'_' -f5 | cut -d'.' -f1) 

	
	if  grep -qs $rollnum Output/AbsenteeList.txt 
	then
		#statements
		echo Found $rollnum

		sed "/$rollnum/d" "Output/AbsenteeList.txt" > Output/TempList.txt
	
		cat Output/TempList.txt > Output/AbsenteeList.txt

		mkdir temp

	
		cp "$zipName" -t temp

		cd temp
		unzip "$zipName" 

		rm "$zipName"

		dirName=$(ls)
		dirCount=$(ls | wc -l)

		#echo $dirCount

		if  [ $dirCount=1 ] && [ $dirName = $rollnum ]; then 
			echo Correct submission by $rollnum #done
			cp "$dirName" -rt ../Output
			echo $rollnum 10 Correct submission >> ../Output/Marks.txt
		elif [ $dirCount -gt 1 ]; then
			echo multiple directory submission by $rollnum #done
 			#cp "$dirName" -rt ../Output
			
			mkdir ../Output/"$rollnum"
			ls | while read dNames 
			do
				cp "$dNames" -rt ../Output/"$rollnum"

			done


			echo $rollnum 0 Multiple directory submission >> ../Output/Marks.txt

		elif [[ $dirName =~ .*$rollnum.* ]]; then
			echo Partially correct submission by $rollnum #done

			mv "$dirName" "$rollnum"
			cp "$rollnum" -rt ../Output
			echo $rollnum 5  Partially correct submission >> ../Output/Marks.txt

		else
			echo Wrong submission by $rollnum
			mv "$dirName" "$rollnum"
			cp "$rollnum" -rt ../Output
			echo $rollnum 0  Wrong submission. Extracted roll from zipName >> ../Output/Marks.txt

		fi

		cd ..

		rm -rf temp
		#rm -f "$zipName"
	elif [ "$zipName" != "Output" ] && [ "$zipName" != "ZipsWithoutRoll" ];then	#zip name without the roll
		echo rollnum not found in the zip name

		cp "$zipName" -t ZipsWithoutRoll
		#rm -f "$zipName"


	fi


done

######################################################################################

echo "Starting zips without roll numbers"

mkdir ZipsWithoutRoll2

cd ZipsWithoutRoll

ls | while read line
do 
	zipName=$line;

	mkdir temp
	
	cp "$zipName" -t temp

	cd temp
	unzip "$zipName" 
	rm "$zipName"

	dirName=$(ls)
	dirCount=$(ls | wc -l)

	if [ $dirCount -gt 1 ]; then
		#multiple directory
		cd ..

		cp "$zipName" -t ../ZipsWithoutRoll2
	else
			if grep -qs $dirName ../../Output/AbsenteeList.txt 
			then 

				sed "/$dirName/d" "../../Output/AbsenteeList.txt" > ../../Output/TempList.txt
				cat ../../Output/TempList.txt > ../../Output/AbsenteeList.txt

				echo Correct submission by $dirName #done
				cp "$dirName" -rt ../../Output
				echo $dirName 10 Correct submission without zipName >> ../../Output/Marks.txt
			
				cd ..
			else
				#wrong folder name 
				cd ..

				cp "$zipName" -t ../ZipsWithoutRoll2

			fi

	fi

	rm  -rf temp

done

cd ..
rm -rf ZipsWithoutRoll


#########################################################################################


cd ZipsWithoutRoll2

ls | while read line
do 
	zipName=$line;
	#echo 5
	Stdname=$(echo $line | cut -d'_' -f1) 
	echo $Stdname

	mkdir temp
	
	cp "$zipName" -t temp

	cd temp
	unzip "$zipName" 
	rm "$zipName"

	dirName=$(ls)
	dirCount=$(ls | wc -l)

	rollnum="NOT FOUND"

	cat ../../../CSE_322.csv | while read entry 
	do
		entryRollNum=$(echo $entry | cut -d',' -f 1 | cut -d'"' -f 2 | cut  -d$'\t' -f2)
		entryName=$(echo $entry | cut -d',' -f2)

		#echo $entryRollNum
		#echo $entryName

		if [ "$entryName" = "$Stdname" ]; then
			if grep -qs $entryRollNum ../../Output/AbsenteeList.txt 
			then
				rollnum2=$entryRollNum
				echo Found $rollnum2

				sed "/$rollnum2/d" "../../Output/AbsenteeList.txt" > ../../Output/TempList.txt
				cat ../../Output/TempList.txt > ../../Output/AbsenteeList.txt

				rm -f ../"$zipName"
				
				if [ $dirCount -gt 1 ]; then
					#multiple directory	

					mkdir ../../Output/"$rollnum2"
					ls | while read dNames 
					do
						cp "$dNames" -rt ../../Output/"$rollnum2"
					done

					echo $rollnum2 0 Multiple directory submission. Extracted roll from CSV >> ../../Output/Marks.txt

				else
					echo Wrong submission by $rollnum2
					mv "$dirName" "$rollnum2"
					cp "$rollnum2" -rt ../../Output
					echo $rollnum2 0  Wrong submission. Extracted roll from CSV >> ../../Output/Marks.txt
				fi


			fi

		fi

	done 

	cd ..

	rm  -rf temp

done

ls | while read line
do
	Stdname=$(echo $line | cut -d'_' -f1) 
	mkdir "$Stdname"
	cp "$line" -rt "$Stdname"
	cd "$Stdname"
	unzip "$line"
	rm -f "$line"
	cd ..
	rm -f rm -f "$line"

done


ls | while read line
do
	cp "$line" -rt ../Output/Extra
done

cd ..
rm -rf ZipsWithoutRoll2

cp Output -rt ../

cd ..

rm -rf workDir